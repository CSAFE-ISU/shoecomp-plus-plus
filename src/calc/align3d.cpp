#include "calc/align3d.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "aligncalc/estimate.h"
#include "aligncalc/rigidLines3d.h"
#include "formats/x3pAnnotationIo.h"

namespace shoecomp
{
    const char* const kNoStriationMatch =
        "no consistent set of striations found";

    namespace
    {
        constexpr double kPi = 3.14159265358979323846;

        enum StriationCol : int
        {
            ColMX,
            ColMY,
            ColMZ,
            ColDX,
            ColDY,
            ColDZ,
            ColLength,
            ColRidge,
            NumStriationCols
        };

        struct StriationRules : AlignCalc::LabelRules
        {
            double maxLengthRatio = 0.0;

            bool admissible(const AlignCalc::DoubleMatrixR& L,
                            int32_t l,
                            const AlignCalc::DoubleMatrixR& R,
                            int32_t r) const
            {
                if (L(l, ColRidge) != R(r, ColRidge)) return false;
                if (maxLengthRatio > 1.0)
                {
                    const double a = L(l, ColLength);
                    const double b = R(r, ColLength);
                    const double lo = std::min(a, b);
                    if (!(lo > 0.0) ||
                        std::max(a, b) > maxLengthRatio * lo)
                        return false;
                }
                return true;
            }
        };

        AlignCalc::LineCols lineCols()
        {
            AlignCalc::LineCols c;
            c.mid = {ColMX, ColMY, ColMZ};
            c.dir = {ColDX, ColDY, ColDZ};
            c.length = ColLength;
            return c;
        }

        Eigen::Vector3d rowMid(const AlignCalc::DoubleMatrixR& m, int i)
        {
            return {m(i, ColMX), m(i, ColMY), m(i, ColMZ)};
        }
        Eigen::Vector3d rowDir(const AlignCalc::DoubleMatrixR& m, int i)
        {
            return Eigen::Vector3d(m(i, ColDX), m(i, ColDY),
                                   m(i, ColDZ))
                .normalized();
        }

        // Whether |T| maps left line |l| onto right line |r|: their
        // directions agree within |angleTol| and the mapped left
        // midpoint lies within |sepTol| of the right line (the offset
        // along the line is not checked; it is often weakly fixed).
        bool explains(const Eigen::Isometry3d& T,
                      const AlignCalc::DoubleMatrixR& L, int l,
                      const AlignCalc::DoubleMatrixR& R, int r,
                      double angleTol, double sepTol)
        {
            const Eigen::Vector3d dl = T.linear() * rowDir(L, l);
            const Eigen::Vector3d dr = rowDir(R, r);
            const double c = std::min(1.0, std::abs(dl.dot(dr)));
            if (std::acos(c) > angleTol) return false;
            const Eigen::Vector3d q = T * rowMid(L, l) - rowMid(R, r);
            return (q - q.dot(dr) * dr).norm() <= sepTol;
        }

        // |g| restricted to the nodes with keep[i] set (ids renumbered
        // in order; nodes left without neighbours are dropped, as in
        // any GraphRows).
        AlignCalc::GraphRows keepNodes(const AlignCalc::GraphRows& g,
                                       const std::vector<char>& keep)
        {
            const size_t n = g.size();
            std::vector<int32_t> remap(n, -1);
            AlignCalc::GraphRows out;
            for (size_t i = 0; i < n; ++i)
            {
                if (!keep[i]) continue;
                bool linked = false;
                for (size_t k = 0; k < g.degree(i) && !linked; ++k)
                    linked = keep[g.row(i)[k]] != 0;
                if (!linked) continue;
                remap[i] = (int32_t)out.nodes.size();
                out.nodes.push_back(g.nodes[i]);
            }
            out.offsets.push_back(0);
            for (size_t i = 0; i < n; ++i)
            {
                if (remap[i] < 0) continue;
                for (size_t k = 0; k < g.degree(i); ++k)
                {
                    const int32_t j = remap[g.row(i)[k]];
                    if (j >= 0) out.adj.push_back(j);
                }
                out.offsets.push_back(out.adj.size());
            }
            return out;
        }
    }  // namespace

    std::vector<StriationLine> surfaceStriations(
        const Surface3D& surf, std::vector<int>* peakIndex)
    {
        std::vector<StriationLine> out;
        if (peakIndex) peakIndex->clear();
        const jt::Json& ann = surf.annotations;
        if (!ann.isObject() || !ann.contains("peaks")) return out;
        const jt::Json& peaks = ann["peaks"];
        if (!peaks.isArray()) return out;
        const auto& arr = peaks.getArray();
        for (size_t k = 0; k < arr.size(); ++k)
        {
            StriationLine ln;
            if (!striationLineFromJson(arr[k], ln)) continue;
            out.push_back(ln);
            if (peakIndex) peakIndex->push_back((int)k);
        }
        return out;
    }

    AlignCalc::DoubleMatrixR striationRows(
        const std::vector<StriationLine>& lines)
    {
        AlignCalc::DoubleMatrixR m((Eigen::Index)lines.size(),
                                   NumStriationCols);
        for (size_t i = 0; i < lines.size(); ++i)
        {
            const StriationLine& s = lines[i];
            const Eigen::Vector3d mid =
                s.start + 0.5 * s.length * s.dir;
            const Eigen::Index r = (Eigen::Index)i;
            m(r, ColMX) = mid.x();
            m(r, ColMY) = mid.y();
            m(r, ColMZ) = mid.z();
            m(r, ColDX) = s.dir.x();
            m(r, ColDY) = s.dir.y();
            m(r, ColDZ) = s.dir.z();
            m(r, ColLength) = s.length;
            m(r, ColRidge) = s.isPeak ? 1.0 : 0.0;
        }
        return m;
    }

    bool runStriationAlign(const AlignCalc::DoubleMatrixR& left,
                           const AlignCalc::DoubleMatrixR& right,
                           const StriationAlignParams& params,
                           WorkerChannel& channel,
                           std::vector<StriationAlignment>& out)
    {
        out.clear();
        const int minLines = std::max(params.minLines, 2);
        if (left.rows() < minLines || right.rows() < minLines)
        {
            channel.error("both lands need detected striations");
            return false;
        }

        AlignCalc::LineParams lp;
        lp.angleTol = params.angleTolDeg * kPi / 180.0;
        lp.sepTol = params.sepTolUm * 1e-6;
        lp.extentScale = params.extentScale;
        StriationRules rules;
        rules.maxLengthRatio = params.maxLengthRatio;
        const AlignCalc::LineCols cols = lineCols();
        AlignCalc::RigidLines3D<StriationRules> model(left, right, cols,
                                                      lp, rules);
        if (!model.valid())
        {
            channel.error("invalid striation lines or tolerances");
            return false;
        }

        AlignCalc::BuildParams bp;
        bp.numWorkers = params.numWorkers;
        AlignCalc::GraphRows graph;
        channel.report(0.25f, "matching striation pairs...");
        if (!AlignCalc::buildGraph(model, bp, channel, graph))
        {
            if (!channel.should_cancel())
                channel.error("could not build the match graph");
            return false;
        }

        // Distinct alignments by peeling: each round takes the
        // largest clique left, fits its transform, then removes every
        // line pair that transform explains, so the next round finds
        // a different alignment rather than a variant of this one
        // (the solver only returns maximum cliques, which for one
        // alignment differ by a few lines at the tolerance edge).
        // A transform rotating more than maxRotationDeg (e.g. one land
        // reversed end-for-end) is peeled but not reported. Alignments
        // after the first reported one need at least min_matched lines
        // and distinctMinFrac of its size. Only the first round reports
        // failures; later ones just stop, with their progress text
        // muted so a final error still fits the channel's ring.
        struct MuteGuard
        {
            WorkerChannel& ch;
            ~MuteGuard() { ch.muteProgress = false; }
        } mute{channel};
        AlignCalc::CliqueParams cp;
        cp.numWorkers = params.numWorkers;
        cp.numResults = 4;  // a few ties, in case a fit fails
        cp.deterministic = true;
        cp.lowerBound = minLines;
        cp.upperBound = (int32_t)std::min(left.rows(), right.rows());
        const double maxRot = params.maxRotationDeg * kPi / 180.0;
        std::vector<char> keep(graph.size(), 1);
        AlignCalc::GraphRows rest;
        const AlignCalc::GraphRows* g = &graph;
        const size_t want = std::max<size_t>(1, params.numResults);
        // Rotated candidates also cost a round; cap them with a
        // constant (not one scaled by |want|) so the first k results
        // are the same whatever |want| is.
        const int kMaxRejected = 16;
        int rejected = 0;
        for (size_t round = 0; out.size() < want; ++round)
        {
            std::vector<std::vector<AlignCalc::Correspondence>> cliques;
            if (round == 0)
                channel.report(0.5f, "searching consistent matches...");
            else
                channel.muteProgress = true;
            if (g->size() == 0 ||
                !AlignCalc::searchCliques(*g, cp, channel, cliques))
            {
                if (channel.should_cancel()) return false;
                if (round > 0) break;
                channel.error(kNoStriationMatch);
                return false;
            }
            if (round == 0)
                channel.report(0.9f, "fitting transforms...");

            StriationAlignment a;
            const std::vector<AlignCalc::Correspondence>* used =
                nullptr;
            for (const auto& clq : cliques)
            {
                AlignCalc::LineFit fit;
                if (!AlignCalc::estimateRigidLines3D(left, right, cols,
                                                     clq, fit))
                    continue;
                a.leftToRight = fit.transform;
                a.axis = fit.axis;
                a.rmsPerp = fit.rmsPerp;
                a.axialWeak = fit.axialWeak;
                for (const auto& c : clq)
                    a.pairs.push_back({c.left, c.right});
                used = &clq;
                break;
            }
            if (!used)
            {
                if (round > 0) break;
                channel.error("transform fit failed");
                return false;
            }

            // Peel: the clique's pairs and every pair the fitted
            // transform explains.
            for (size_t i = 0; i < graph.size(); ++i)
            {
                if (!keep[i]) continue;
                const AlignCalc::Correspondence c = graph.nodes[i];
                if (std::binary_search(used->begin(), used->end(), c) ||
                    explains(a.leftToRight, left, c.left, right,
                             c.right, lp.angleTol, lp.sepTol))
                    keep[i] = 0;
            }
            const bool rotated =
                maxRot > 0.0 &&
                Eigen::AngleAxisd(a.leftToRight.rotation()).angle() >
                    maxRot;
            if (rotated && ++rejected > kMaxRejected) break;
            if (!rotated)
            {
                if (out.empty())
                    cp.lowerBound = std::max(
                        minLines,
                        (int32_t)std::ceil(params.distinctMinFrac *
                                               (double)used->size() -
                                           1e-9));
                out.push_back(std::move(a));
            }
            if (out.size() >= want || channel.should_cancel()) break;
            rest = keepNodes(graph, keep);
            g = &rest;
        }
        if (channel.should_cancel()) return false;
        if (out.empty())
        {
            // every candidate rotated past maxRotationDeg
            channel.error(kNoStriationMatch);
            return false;
        }
        return true;
    }
}  // namespace shoecomp
