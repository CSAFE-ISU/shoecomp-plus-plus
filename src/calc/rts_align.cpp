#include "calc/align.h"
#include "core/pointType.h"
#include <algorithm>
#include <cstdio>

namespace shoecomp
{
    enum PointCol : int
    {
        ColX,
        ColY,
        ColType,
        NumCols
    };

    static bool extractAnnotatedPoints(const jt::Json& annotations,
                                       AlignCalc::DoubleMatrixR& mat)
    {
        const jt::Json& ann = annotations;
        if (!ann.isObject() || !ann.contains("points") ||
            !ann["points"].isArray())
        {
            return false;
        }
        auto& pts = ann["points"].getArray();
        if (pts.size() == 0) { return false; }
        if (pts.size() > AlignCalc::MAX_POINTS) { return false; }
        mat.resize(pts.size(), NumCols);
        for (size_t i = 0; i < pts.size(); ++i)
        {
            auto& el = pts[i];
            if (!el.isObject()) return false;
            if (!el.contains("x") || !el["x"].isNumber()) return false;
            if (!el.contains("y") || !el["y"].isNumber()) return false;
            if (!el.contains("type") || !el["type"].isString())
                return false;
            mat(i, ColX) = el["x"].getNumber();
            mat(i, ColY) = el["y"].getNumber();
            mat(i, ColType) = static_cast<double>(
                stringToPointType(el["type"].getString()));
        }
        return true;
    }

    void runRTSAlign(const jt::Json& leftAnn, const jt::Json& rightAnn,
                     WorkerChannel& channel,
                     std::vector<AlignState>& aligns,
                     const AlignCalc::RTSParams& params,
                     std::vector<MatchedRows>* matched)
    {
        AlignCalc::DoubleMatrixR left_pts;
        AlignCalc::DoubleMatrixR right_pts;
        std::vector<AlignCalc::MatchedPoints> results;

        channel.report(0.0f, "loading points...");
        if (!extractAnnotatedPoints(leftAnn, left_pts))
        {
            channel.error(
                "left image does not have 1 to 256 valid points");
            channel.cancelled();
            return;
        }
        if (!extractAnnotatedPoints(rightAnn, right_pts))
        {
            channel.error(
                "right image does not have 1 to 256 valid points");
            channel.cancelled();
            return;
        }

        AlignCalc::RTSParams run = params;
        if (run.upperBound <= 0)
        {
            run.upperBound = std::max(
                3, (int)std::min(left_pts.rows(), right_pts.rows()));
        }

        AlignCalc::PointCols cols;
        cols.dim = 2;
        cols.pos = {ColX, ColY, 0};
        if (!AlignCalc::getAlignment(left_pts, right_pts, cols, ColType,
                                     run, channel, results))
        {
            channel.cancelled();
            return;
        }

        AlignCalc::RTSTransform tform;
        for (const auto& match : results)
        {
            tform.estimate(match, true);

            double scale = params.sameScale ? 1.0 : tform.scale;
            if (!params.sameScale && params.scaleRange > 0.0)
            {
                double lo = 1.0 / params.scaleRange;
                double hi = params.scaleRange;
                if (scale < lo || scale > hi) continue;
            }

            aligns.push_back(AlignState{});
            aligns.back().rotation = tform.rotation /* in radians */;
            aligns.back().dx = tform.dx;
            aligns.back().dy = tform.dy;
            aligns.back().scale = scale;
            aligns.back().mode = AlignMode::Automatic;
            if (matched) matched->push_back(match.pairs);

            // Store matched points in info
            aligns.back().info.setObject();
            aligns.back().info["leftPoints"].setArray();
            aligns.back().info["rightPoints"].setArray();
            for (int32_t i = 0; i < match.N; ++i)
            {
                jt::Json lpt;
                lpt.setObject();
                lpt["x"] = match.left_pts(i, 0);
                lpt["y"] = match.left_pts(i, 1);
                aligns.back().info["leftPoints"].getArray().push_back(
                    std::move(lpt));

                jt::Json rpt;
                rpt.setObject();
                rpt["x"] = match.right_pts(i, 0);
                rpt["y"] = match.right_pts(i, 1);
                aligns.back().info["rightPoints"].getArray().push_back(
                    std::move(rpt));
            }
        }
        char buf[64];
        snprintf(buf, sizeof(buf), "%zu alignment(s) added",
                 aligns.size());
        channel.report(1.0f, buf);
        channel.done();
    }
}  // namespace shoecomp
