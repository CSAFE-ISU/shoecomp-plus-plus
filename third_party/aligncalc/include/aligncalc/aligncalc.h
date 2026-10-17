#ifndef ALIGNCALC_H
#define ALIGNCALC_H

#include "Eigen/Geometry"
#include "alignTypes.h"
#include "correspondence.h"
#include "distinct.h"
#include "estimate.h"
#include "progress.h"
#include "rigidLines3d.h"
#include "rules.h"
#include <cstdint>
#include <string>
#include <vector>

// General rigid (RT) and similarity (RTS) matching. Inputs are
// row-major matrices, one row per point or line, with the columns
// named by PointCols / LineCols. Matching is by correspondence graph
// and maximum cliques, so it needs no initial guess; every result is
// a distinct alignment (see searchDistinct), best first.
namespace AlignCalc
{
    // Clique-search options shared by every entry point.
    struct CliqueOptions
    {
        int32_t minMatched = 3;  // fewer matched pairs is no match
        int32_t maxMatched = 0;  // 0 = fewer of the two counts
        size_t results = 2;      // distinct alignments, best first
        DistinctMode distinct = DistinctMode::Explained;
        // Alignments after the first need this fraction of its size
        // (and minMatched).
        double distinctMinFrac = 0.7;
        size_t numWorkers = 0;  // 0 = std::thread::hardware_concurrency
    };

    enum class PointMode
    {
        Rigid,       // RT, points in 1-3 dimensions
        Similarity,  // RTS, 2-D points
    };

    struct PointOptions
    {
        PointMode mode = PointMode::Rigid;
        // Distances may differ by this fraction (relative error).
        double epsilon = 0.005;
        // Similarity: scales within [1/scaleRange, scaleRange].
        double scaleRange = 5.0;
        // 2-D anchored clique search (PointAnchor2D) with this
        // relative tolerance; 0 = off.
        double anchorTol = 0.05;
        // Similarity: an edge needs this many matched triangles.
        int32_t minSupport = 1;
        // Similarity with PointCols::angle: skip triangle pairs whose
        // orientations disagree by more than this (degrees); 0 = off.
        double orientationTolDeg = 0.0;
    };

    struct PointAlignment
    {
        // matched rows, left -> right
        std::vector<Correspondence> pairs;
        // T(left) ~= right; scale 1 for PointMode::Rigid
        PointTransform transform;
        // pairs the alignment accounts for (see searchDistinct)
        int32_t explained = 0;
    };

    struct PointResult
    {
        Status status = Status::Error;
        std::string message;  // set unless status is Ok
        std::vector<PointAlignment> alignments;
    };

    // Point matching; only equal labels match when cols.label >= 0.
    // A similarity fit outside options.scaleRange is set aside.
    PointResult alignPoints(const DoubleMatrixR& left,
                            const DoubleMatrixR& right,
                            const PointCols& cols,
                            const PointOptions& options,
                            const CliqueOptions& clique,
                            Progress* progress = nullptr);

    struct LineAlignment
    {
        // matched rows, left -> right
        std::vector<Correspondence> pairs;
        Eigen::Isometry3d transform = Eigen::Isometry3d::Identity();
        // see LineFit
        Eigen::Vector3d axis = Eigen::Vector3d::UnitZ();
        double rmsPerp = 0.0;
        bool axialWeak = false;
        // pairs the alignment accounts for (see searchDistinct)
        int32_t explained = 0;
    };

    struct LineResult
    {
        Status status = Status::Error;
        std::string message;  // set unless status is Ok
        std::vector<LineAlignment> alignments;
    };

    // Rigid matching of 3-D line segments (RigidLines3D); only equal
    // labels match when cols.label >= 0.
    LineResult alignLines3D(const DoubleMatrixR& left,
                            const DoubleMatrixR& right,
                            const LineCols& cols,
                            const LineParams& params,
                            const CliqueOptions& clique,
                            Progress* progress = nullptr);

}  // namespace AlignCalc

#endif
