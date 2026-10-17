#include "core/pipeline.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstring>
#include <exception>
#include <filesystem>
#include <thread>

#include "calc/onnxRuntime.h"
#include "core/alignmentJson.h"
#include "formats/annotationIo.h"
#include "formats/ebts.h"
#include "formats/imageDecode.h"
#include "formats/x3p.h"
#include "formats/x3pAnnotationIo.h"

namespace fs = std::filesystem;

namespace shoecomp
{
    namespace
    {
        std::string lowerExt(const std::string& path)
        {
            std::string e = fs::path(path).extension().string();
            for (char& c : e) c = (char)std::tolower((unsigned char)c);
            return e;
        }

        // An EBTS/NIST transaction (vs a plain PNG under the EBTS
        // type).
        bool isTransactionFile(const std::string& path)
        {
            const std::string e = lowerExt(path);
            return e == ".an2" || e == ".irr" || e == ".lffs" ||
                   e == ".ebts";
        }

        void say(const RunOptions& o, const std::string& msg)
        {
            if (o.log) o.log(msg);
        }

        std::string timeoutText(const RunOptions& o)
        {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "timed out after %g s",
                          o.timeoutSec);
            return buf;
        }

        jt::Json emptyAnnotations()
        {
            jt::Json a;
            a.setObject();
            a["bounds"].setArray();
            a["points"].setArray();
            return a;
        }

        bool loadEBTSRecord(const Item& it, EBTSImage& out,
                            std::string& err)
        {
            std::vector<EBTSImage> recs;
            if (parseEBTS(it.path, recs, err) <= 0) return false;
            if (it.record < 0 || it.record >= (int)recs.size())
            {
                err = it.path + " has " + std::to_string(recs.size()) +
                      " image record(s); no record @" +
                      std::to_string(it.record);
                return false;
            }
            out = std::move(recs[it.record]);
            return true;
        }

        bool typesAllowed(const Item& it, const jt::Json& ann,
                          std::string& err)
        {
            const auto allowed = pointTypesFor(it.type);
            for (const auto& pt : ann["points"].getArray())
            {
                if (!pt.contains("type")) continue;
                const std::string& s = pt["type"].getString();
                if (std::find(allowed.begin(), allowed.end(),
                              stringToPointType(s)) == allowed.end())
                {
                    err = "point type '" + s + "' is not allowed for " +
                          imageTypeName(it.type) + " inputs";
                    return false;
                }
            }
            return true;
        }

        bool load2DFile(const Item& it, const std::string& path,
                        jt::Json& ann, std::string& err)
        {
            if (loadAnnotationsFromFile(path, ann) != 0)
            {
                err = "cannot read annotations from " + path;
                return false;
            }
            return typesAllowed(it, ann, err);
        }

        // Runs |work| on a worker thread while draining |ch| here, so
        // no message is dropped from the fixed-size ring and progress
        // can be logged live. Past opts.timeoutSec it sets the
        // channel's cancel flag (which the calculations poll) and
        // |timedOut|, then waits for |work| to return. Errors come
        // back through |work|'s own results, not the channel.
        template <typename F>
        void runDraining(WorkerChannel& ch, const std::string& label,
                         const RunOptions& opts, bool& timedOut,
                         F&& work)
        {
            timedOut = false;
            const auto start = std::chrono::steady_clock::now();
            std::string lastText;
            auto drain = [&]
            {
                while (auto m = ch.messages.pop())
                {
                    if ((m->kind == MsgKind::Progress ||
                         m->kind == MsgKind::Log) &&
                        m->text[0] != '\0' && lastText != m->text)
                    {
                        lastText = m->text;
                        say(opts, label + ": " + lastText);
                    }
                }
            };
            std::atomic<bool> finished{false};
            std::exception_ptr thrown;
            ch.is_running.store(true);
            std::thread t(
                [&]
                {
                    try
                    {
                        work();
                    }
                    catch (...)
                    {
                        thrown = std::current_exception();
                    }
                    finished.store(true);
                });
            while (!finished.load())
            {
                drain();
                const double elapsed =
                    std::chrono::duration<double>(
                        std::chrono::steady_clock::now() - start)
                        .count();
                if (opts.timeoutSec > 0.0 && !timedOut &&
                    elapsed > opts.timeoutSec)
                {
                    timedOut = true;
                    ch.cancel_requested.store(true);
                    say(opts, label + ": timed out, cancelling...");
                }
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(5));
            }
            t.join();
            drain();
            if (thrown) std::rethrow_exception(thrown);
        }

        bool detect2D(const Item& it, const Params& params,
                      const RunOptions& opts, jt::Json& ann,
                      bool& timedOut, std::string& err)
        {
            OnnxRuntime& ort = OnnxRuntime::instance();
            if (!ort.ensureLoaded())
            {
                err =
                    "onnxruntime is not available: " + ort.lastError();
                return false;
            }
            std::vector<unsigned char> rgba;
            int w = 0, h = 0;
            if (!loadItemPixels(it, rgba, w, h, err)) return false;

            const DetectionSpec spec = detectionSpecFor(it.type);
            DetectionInput in;
            in.rgba = rgba.data();
            in.imgW = w;
            in.imgH = h;
            in.inputW = spec.inputWidth;
            in.inputH = spec.inputHeight;
            in.nchw = spec.nchw;
            in.normalize = spec.normalize;
            in.confThreshold = params.detect2d.confThreshold;
            in.iouThreshold = params.detect2d.iouThreshold;
            in.numThreads = resolveThreads(opts.threads);

            std::vector<Detection> dets;
            WorkerChannel ch;
            bool ok = false;
            std::string runErr;
            runDraining(ch, it.label, opts, timedOut,
                        [&]
                        {
                            ok = runYoloDetection(params.detect2d.model,
                                                  in, dets, runErr,
                                                  &ch.cancel_requested);
                        });
            if (timedOut)
            {
                err = "point detection " + timeoutText(opts);
                return false;
            }
            if (!ok)
            {
                err = runErr;
                return false;
            }

            ann = emptyAnnotations();
            auto& pts = ann["points"].getArray();
            const int nTypes = (int)spec.classToPointType.size();
            for (const auto& d : dets)
            {
                if (d.cls < 0 || d.cls >= nTypes) continue;
                jt::Json pt;
                pt.setObject();
                pt["x"] = d.cx;
                pt["y"] = d.cy;
                pt["type"] =
                    pointTypeToString(spec.classToPointType[d.cls]);
                pts.push_back(std::move(pt));
            }
            return true;
        }

        bool saveDetected(const Item& it, const RunOptions& opts,
                          std::string& err)
        {
            if (opts.saveAnnotationsDir.empty()) return true;
            std::error_code ec;
            fs::create_directories(opts.saveAnnotationsDir, ec);
            const fs::path inDir = fs::path(it.path).parent_path();
            if (fs::equivalent(opts.saveAnnotationsDir,
                               inDir.empty() ? "." : inDir, ec))
            {
                err =
                    "refusing to save detected annotations next to "
                    "the inputs (would replace sidecars): " +
                    opts.saveAnnotationsDir;
                return false;
            }
            const std::string out =
                (fs::path(opts.saveAnnotationsDir) /
                 (it.saveName.empty() ? annotationFileName(it)
                                      : it.saveName))
                    .string();
            const int rc =
                is3DType(it.type)
                    ? saveX3PAnnotations(out, it.annotations)
                    : saveAnnotationsToFile(out, it.annotations);
            if (rc != 0)
            {
                err = "cannot write " + out;
                return false;
            }
            return true;
        }

        bool prepare2D(Item& it, const Params& params,
                       const RunOptions& opts, bool& detected)
        {
            std::string& err = it.error;
            const bool hasModel = !params.detect2d.model.empty();
            const std::string side = sidecarPath(it);
            if (!it.annPath.empty())
            {
                it.annSource = it.annPath;
                return load2DFile(it, it.annPath, it.annotations, err);
            }
            if (!opts.redetect && fs::exists(side))
            {
                it.annSource = side;
                return load2DFile(it, side, it.annotations, err);
            }
            if (!opts.redetect && it.record >= 0)
            {
                EBTSImage rec;
                if (!loadEBTSRecord(it, rec, err)) return false;
                if (!rec.annotations["points"].getArray().empty() ||
                    !hasModel)
                {
                    it.annSource = "EBTS Type-9";
                    it.annotations = std::move(rec.annotations);
                    return true;
                }
            }
            if (!hasModel)
            {
                err = opts.redetect
                          ? "--redetect needs a detection model "
                            "(--detect2d-model)"
                          : "no annotations: no " + side +
                                " and no detection model "
                                "(--detect2d-model)";
                return false;
            }
            say(opts, it.label + ": detecting points...");
            if (!detect2D(it, params, opts, it.annotations, it.timedOut,
                          err))
                return false;
            it.annSource = "detected";
            detected = true;
            return true;
        }

        bool prepare3D(Item& it, const Params& params,
                       const RunOptions& opts, bool& detected)
        {
            std::string& err = it.error;
            jt::Json base;
            base.setObject();
            std::string from;
            if (!it.annPath.empty())
                from = it.annPath;
            else if (!opts.redetect && fs::exists(sidecarPath(it)))
                from = sidecarPath(it);
            if (!from.empty() && loadX3PAnnotations(from, base) != 0)
            {
                err = "cannot read annotations from " + from;
                return false;
            }
            const bool hasPeaks = base.contains("peaks") &&
                                  base["peaks"].isArray() &&
                                  !base["peaks"].getArray().empty();
            if (hasPeaks) { it.annSource = from; }
            else
            {
                Surface3D surf;
                if (!loadX3P(it.path, surf, err))
                {
                    err = it.path + ": " + err;
                    return false;
                }
                say(opts, it.label + ": detecting striations...");
                std::vector<StriationLine> lines;
                StriationParams sp = params.striation.toParams(
                    resolveThreads(opts.threads));
                WorkerChannel ch;
                bool ok = false, timedOut = false;
                std::string runErr;
                runDraining(ch, it.label, opts, timedOut,
                            [&]
                            {
                                ok = detectStriations(
                                    surf, sp, lines, runErr,
                                    &ch.cancel_requested,
                                    [&ch](float p, const char* t)
                                    { ch.report(p, t); });
                            });
                if (timedOut || !ok)
                {
                    it.timedOut = timedOut;
                    err = "striation detection " +
                          (timedOut ? timeoutText(opts) : runErr);
                    return false;
                }
                base["peaks"].setArray();
                for (const auto& l : lines)
                    base["peaks"].getArray().push_back(
                        striationLineToJson(l));
                it.annSource = "detected";
                detected = true;
            }
            it.annotations = std::move(base);

            Surface3D view;
            view.annotations = it.annotations;
            const std::vector<StriationLine> lines =
                surfaceStriations(view, &it.peakIdx);
            it.rows = striationRows(lines);
            it.count = lines.size();
            return true;
        }

    }  // namespace

    static bool prepareItemImpl(Item& it, const Params& params,
                                const RunOptions& opts);
    static PairResult comparePairImpl(const Item& left,
                                      const Item& right,
                                      const Params& params,
                                      const RunOptions& opts);

    const char* pairStatusName(PairStatus s)
    {
        switch (s)
        {
            case PairStatus::Ok:
                return "ok";
            case PairStatus::NoMatch:
                return "none";
            case PairStatus::Timeout:
                return "timeout";
            case PairStatus::Error:
            default:
                return "error";
        }
    }

    int resolveThreads(int threads)
    {
        if (threads > 0) return threads;
        return (int)std::max(1u, std::thread::hardware_concurrency());
    }

    bool parseItemSpec(ImageType type, const std::string& spec,
                       Item& out, std::string& err)
    {
        out = Item{};
        out.type = type;
        out.path = spec;
        const size_t at = spec.rfind('@');
        if (type == ImageType::EBTS && at != std::string::npos &&
            at + 1 < spec.size() &&
            std::all_of(spec.begin() + at + 1, spec.end(),
                        [](char c)
                        { return std::isdigit((unsigned char)c); }))
        {
            const std::string num = spec.substr(at + 1);
            if (num.size() > 6)
            {
                err = "record index out of range: " + spec;
                return false;
            }
            out.path = spec.substr(0, at);
            out.record = std::stoi(num);
        }
        if (!fs::is_regular_file(out.path))
        {
            err = "no such file: " + out.path;
            return false;
        }
        if (type == ImageType::EBTS && out.record < 0 &&
            isTransactionFile(out.path))
        {
            std::vector<EBTSImage> recs;
            if (parseEBTS(out.path, recs, err) <= 0) return false;
            if (recs.size() != 1)
            {
                err = out.path + " has " + std::to_string(recs.size()) +
                      " image records; pick one with " + out.path +
                      "@N (0-based)";
                return false;
            }
            out.record = 0;
        }
        out.label = fs::path(out.path).filename().string();
        if (out.record >= 0)
            out.label += "@" + std::to_string(out.record);
        return true;
    }

    bool itemsForFile(ImageType type, const std::string& path,
                      std::vector<Item>& out, std::string& err)
    {
        Item it;
        it.type = type;
        it.path = path;
        it.label = fs::path(path).filename().string();
        if (type == ImageType::EBTS && isTransactionFile(path))
        {
            std::vector<EBTSImage> recs;
            if (parseEBTS(path, recs, err) <= 0) return false;
            for (size_t i = 0; i < recs.size(); ++i)
            {
                Item r = it;
                r.record = (int)i;
                r.label += "@" + std::to_string(i);
                out.push_back(std::move(r));
            }
            return true;
        }
        out.push_back(std::move(it));
        return true;
    }

    std::string annotationFileName(const Item& it)
    {
        std::string stem = fs::path(it.path).stem().string();
        if (it.record >= 0) stem += "@" + std::to_string(it.record);
        return stem + ".json";
    }

    std::string sidecarPath(const Item& it)
    {
        return (fs::path(it.path).parent_path() /
                annotationFileName(it))
            .string();
    }

    namespace
    {
        std::string describe(const std::exception_ptr& e)
        {
            try
            {
                std::rethrow_exception(e);
            }
            catch (const std::exception& x)
            {
                return std::string("unexpected error: ") + x.what();
            }
            catch (...)
            {
                return "unexpected error";
            }
        }
    }  // namespace

    bool prepareItem(Item& it, const Params& params,
                     const RunOptions& opts)
    {
        try
        {
            return prepareItemImpl(it, params, opts);
        }
        catch (...)
        {
            it.prepared = false;
            it.error = describe(std::current_exception());
            return false;
        }
    }

    PairResult comparePair(const Item& left, const Item& right,
                           const Params& params, const RunOptions& opts)
    {
        try
        {
            return comparePairImpl(left, right, params, opts);
        }
        catch (...)
        {
            PairResult res;
            res.alignments.setArray();
            res.status = PairStatus::Error;
            res.message = describe(std::current_exception());
            return res;
        }
    }

    static bool prepareItemImpl(Item& it, const Params& params,
                                const RunOptions& opts)
    {
        it.prepared = false;
        it.timedOut = false;
        it.error.clear();
        it.count = 0;
        bool detected = false;
        const bool ok = is3DType(it.type)
                            ? prepare3D(it, params, opts, detected)
                            : prepare2D(it, params, opts, detected);
        if (!ok) return false;
        if (!is3DType(it.type))
            it.count = it.annotations["points"].getArray().size();
        if (detected && !saveDetected(it, opts, it.error)) return false;
        say(opts, it.label + ": " + std::to_string(it.count) +
                      (is3DType(it.type) ? " striation(s) from "
                                         : " point(s) from ") +
                      it.annSource);
        it.prepared = true;
        return true;
    }

    static PairResult comparePairImpl(const Item& left,
                                      const Item& right,
                                      const Params& params,
                                      const RunOptions& opts)
    {
        PairResult res;
        res.alignments.setArray();
        const auto t0 = std::chrono::steady_clock::now();
        auto finish = [&]
        {
            res.timeMs = std::chrono::duration<double, std::milli>(
                             std::chrono::steady_clock::now() - t0)
                             .count();
            return res;
        };
        if (!left.prepared || !right.prepared)
        {
            const Item& bad = !left.prepared ? left : right;
            res.message = bad.error;
            if (bad.timedOut) res.status = PairStatus::Timeout;
            return finish();
        }
        const std::string label = left.label + " vs " + right.label;
        WorkerChannel ch;

        if (!is3DType(left.type))
        {
            AlignCalc::RTSParams p = params.rtsParams();
            p.numWorkers = (size_t)resolveThreads(opts.threads);
            std::vector<MatchedRows> rows;
            bool timedOut = false;
            AlignCalc::Status status = AlignCalc::Status::Error;
            std::string err;
            runDraining(ch, label, opts, timedOut,
                        [&]
                        {
                            PointMatchRules rules;
                            rules.type = left.type;
                            rules.minutiaAngleTolDeg =
                                params.minutiaAngleTolDeg;
                            status = runAutoAlign(
                                left.annotations, right.annotations, ch,
                                res.align2d, p, rules, &rows, &err);
                        });
            for (size_t i = 0; i < res.align2d.size(); ++i)
            {
                res.alignments.getArray().push_back(
                    alignment2DToJson(res.align2d[i]));
                std::vector<std::pair<int, int>> m;
                for (const auto& c : rows[i])
                    m.push_back({c.left, c.right});
                res.matched.push_back(std::move(m));
                const jt::Json& info = res.align2d[i].info;
                res.explained.push_back(
                    info.contains("explained")
                        ? (int)info["explained"].getNumber()
                        : (int)rows[i].size());
            }
            if (timedOut)
            {
                res = PairResult{};
                res.alignments.setArray();
                res.status = PairStatus::Timeout;
                res.message = timeoutText(opts);
            }
            else if (!res.align2d.empty())
                res.status = PairStatus::Ok;
            else if (status == AlignCalc::Status::Ok ||
                     status == AlignCalc::Status::NoMatch)
            {
                // Ok with no result: every fit outside the scale range
                res.status = PairStatus::NoMatch;
                res.message = err.empty() ? "no alignment found" : err;
            }
            else
                res.message = err;
            return finish();
        }

        const Align3DSettings& s = params.align3d;
        const int minLines = params.clique.minMatched;
        if ((int)left.count < minLines || (int)right.count < minLines)
        {
            res.status = PairStatus::NoMatch;
            res.message = "need at least " + std::to_string(minLines) +
                          " striations on each land (found " +
                          std::to_string(left.count) + " and " +
                          std::to_string(right.count) + ")";
            return finish();
        }
        const StriationAlignParams p = s.toParams(
            (size_t)resolveThreads(opts.threads), params.clique);
        bool timedOut = false;
        AlignCalc::Status status = AlignCalc::Status::Error;
        std::string err;
        runDraining(ch, label, opts, timedOut,
                    [&]
                    {
                        status =
                            runStriationAlign(left.rows, right.rows, p,
                                              ch, res.align3d, &err);
                    });
        if (timedOut)
        {
            res.align3d.clear();
            res.status = PairStatus::Timeout;
            res.message = timeoutText(opts);
            return finish();
        }
        if (status != AlignCalc::Status::Ok)
        {
            res.align3d.clear();
            res.message = err.empty() ? "alignment failed" : err;
            res.status = status == AlignCalc::Status::NoMatch
                             ? PairStatus::NoMatch
                             : PairStatus::Error;
            return finish();
        }
        for (const auto& a : res.align3d)
        {
            res.alignments.getArray().push_back(automatic3DToJson(
                a.leftToRight,
                striationMatchInfo(a, left.peakIdx, right.peakIdx, s)));
            std::vector<std::pair<int, int>> m;
            for (const auto& pr : a.pairs)
                m.push_back(
                    {left.peakIdx[pr.first], right.peakIdx[pr.second]});
            res.matched.push_back(std::move(m));
            res.explained.push_back(a.explained);
        }
        res.status = PairStatus::Ok;
        return finish();
    }

    bool loadItemPixels(const Item& it,
                        std::vector<unsigned char>& rgba, int& w,
                        int& h, std::string& err)
    {
        if (it.record >= 0)
        {
            EBTSImage rec;
            if (!loadEBTSRecord(it, rec, err)) return false;
            return decodeImageMemory(rec.encoded.data(),
                                     rec.encoded.size(), rgba, w, h,
                                     err);
        }
        return decodeImageFile(it.path, rgba, w, h, err);
    }
}  // namespace shoecomp
