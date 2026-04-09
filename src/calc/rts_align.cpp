#include "ui/alignDialog.h"
#include "ui/imageCanvas.h"
#include "calc/align.h"
#include <chrono>
#include <cstdio>
#include <thread>
#include <iostream>

namespace shoecomp
{
    static bool extractAnnotatedPoints(ImageData& data,
                                       AlignCalc::DoubleMatrixR& mat)
    {
        if (!data.annotations.isObject() ||
            !data.annotations.contains("points") ||
            !data.annotations["points"].isArray())
        {
            return false;
        }
        auto& pts = data.annotations["points"].getArray();
        if (pts.size() == 0) { return false; }
        if (pts.size() > AlignCalc::MAX_POINTS) { return false; }
        mat.resize(pts.size(), 3);
        for (size_t i = 0; i < pts.size(); ++i)
        {
            auto& el = pts[i];
            if (!el.isObject()) return false;
            if (!el.contains("x") || !el["x"].isNumber()) return false;
            if (!el.contains("y") || !el["y"].isNumber()) return false;
            if (!el.contains("type") || !el["type"].isString())
                return false;
            mat(i, 0) = el["x"].getNumber();
            mat(i, 1) = el["y"].getNumber();
            mat(i, 2) = static_cast<double>(
                stringToPointType(el["type"].getString()));
        }
        return true;
    }

    void runRTSAlign(ImageData& left, ImageData& right,
                     WorkerChannel& channel,
                     std::vector<AlignState>& aligns,
                     const AlignCalc::RTSParams& params)
    {
        AlignCalc::DoubleMatrixR left_pts;
        AlignCalc::DoubleMatrixR right_pts;
        std::vector<AlignCalc::MatchedPoints> results;

        channel.report(0.0f, "loading points...");
        if (!extractAnnotatedPoints(left, left_pts))
        {
            channel.error(
                "left image does not have 1 to 256 valid points");
            channel.cancelled();
            return;
        }
        if (!extractAnnotatedPoints(right, right_pts))
        {
            channel.error(
                "right image does not have 1 to 256 valid points");
            channel.cancelled();
            return;
        }

        if (!AlignCalc::RTSAlignment(left_pts, right_pts, params,
                                     channel, results))
        {
            channel.cancelled();
            return;
        }

        printf("fitting tforms...\n");
        AlignCalc::RTSTransform tform;
        for (const auto& match : results)
        {
            tform.estimate(match, true);
            aligns.push_back(AlignState{});
            aligns.back().rotation = tform.rotation /* in radians */;
            aligns.back().dx = tform.dx;
            aligns.back().dy = tform.dy;
            aligns.back().scale = tform.scale;
            aligns.back().mode = AlignMode::Automatic;

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
        printf("fitted %ld tforms...\n", aligns.size());

        char buf[64];
        snprintf(buf, sizeof(buf), "%zu alignment(s) added",
                 aligns.size());
        channel.report(1.0f, buf);
        channel.done();
    }
}  // namespace shoecomp
