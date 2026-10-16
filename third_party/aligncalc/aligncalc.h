#ifndef ALIGNCALC_H
#define ALIGNCALC_H

#include "Eigen/Dense"
#include "alignTypes.h"
#include "correspondence.h"
#include "distinct.h"
#include "rules.h"
#include "workerChannel.h"
#include <cstdint>
#include <vector>

namespace AlignCalc
{

    struct RTSParams
    {
        double scaleRange;
        double epsilon;
        size_t numWorkers;
        size_t numResults;
        int32_t lowerBound;
        int32_t upperBound;
        bool sameScale;
        // Similarity only: an edge needs this many matched triangles.
        int32_t minSupport;
        // Similarity only: oriented-point prefilter, see
        // SimilarityParams (angleCol < 0 = off).
        int32_t angleCol;
        double angleTolDeg;
        // Anchored clique search (PointAnchor2D; scale range 1
        // when sameScale) with this relative tolerance; 0 = off.
        double anchorTol;
        // How later results are kept apart (see searchDistinct).
        DistinctMode distinct;

        RTSParams()
        {
            scaleRange = 5.0;
            epsilon = 0.005;
            numWorkers = 2;
            numResults = 2;
            lowerBound = 3;
            upperBound = 500;
            sameScale = false;
            minSupport = 1;
            angleCol = -1;
            angleTolDeg = 0.0;
            anchorTol = 0.05;
            distinct = DistinctMode::Explained;
        }
    };

    // left_pts / right_pts hold the matched positions (the
    // PointCols columns, in order); pairs are the row indices.
    struct MatchedPoints
    {
        int32_t N;
        DoubleMatrixR left_pts;
        DoubleMatrixR right_pts;
        std::vector<Correspondence> pairs;
        // pairs the alignment accounts for (see searchDistinct)
        int32_t explained = 0;
    };

    struct RTSTransform
    {
        double scale;
        double rotation;
        double dx;
        double dy;

        void fromMatrix(const DoubleMatrixR &);
        void toMatrix(DoubleMatrixR &) const;
        void estimate(const MatchedPoints &, bool);
    };

    // Error text the 2-D alignment posts when no clique reaches
    // params.lowerBound (a non-match rather than a failure).
    extern const char *const kNoPointMatch;

    // 2-D alignment: rigid (RigidPoints) when params.sameScale, else
    // similarity (SimilarityPoints2D). Positions come from |cols|;
    // labelCol >= 0 restricts matches to equal labels.
    bool getAlignment(const DoubleMatrixR &, const DoubleMatrixR &,
                      const PointCols &, int labelCol,
                      const RTSParams &, WorkerChannel &,
                      std::vector<MatchedPoints> &);

}  // namespace AlignCalc

#endif
