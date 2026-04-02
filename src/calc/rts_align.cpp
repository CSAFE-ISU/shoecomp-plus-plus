#include "ui/alignDialog.h"
#include "ui/imageCanvas.h"
#include "calc/align.h"
#include <chrono>
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
                     WorkerChannel& channel, AlignResult& result,
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
            channel.error("alignment failed");
            channel.cancelled();
            return;
        }

        //
        channel.done();
    }
}  // namespace shoecomp
