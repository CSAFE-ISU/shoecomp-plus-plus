#ifndef ALIGNCALC_H
#define ALIGNCALC_H

#include "Eigen/Dense"
#include "alignTypes.h"
#include "correspondence.h"
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

        RTSParams()
        {
            scaleRange = 5.0;
            epsilon = 0.005;
            numWorkers = 2;
            numResults = 2;
            lowerBound = 3;
            upperBound = 500;
            sameScale = false;
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

    // 2-D alignment: rigid (RigidPoints) when params.sameScale, else
    // similarity (SimilarityPoints2D). Positions come from |cols|;
    // labelCol >= 0 restricts matches to equal labels.
    bool getAlignment(const DoubleMatrixR &, const DoubleMatrixR &,
                      const PointCols &, int labelCol,
                      const RTSParams &, WorkerChannel &,
                      std::vector<MatchedPoints> &);

}  // namespace AlignCalc

#endif
