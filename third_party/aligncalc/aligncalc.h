#ifndef ALIGNCALC_H
#define ALIGNCALC_H

#include "Eigen/Dense"
#include "workerChannel.h"
#include "multiQueue.h"
#include <cstdint>
#include <vector>
#include <unordered_set>

namespace AlignCalc
{

    static constexpr size_t MAX_POINTS = 256;
    using DoubleMatrixR =
        Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic,
                      Eigen::RowMajor>;

    struct RTSParams
    {
        double delta;
        double epsilon;
        size_t numWorkers;
        size_t numResults;
        int32_t lowerBound;
        int32_t upperBound;
        bool sameScale;
    };

    struct MatchedPoints
    {
        int32_t N;
        DoubleMatrixR left_pts;
        DoubleMatrixR right_pts;
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

    bool getAlignment(const DoubleMatrixR &, const DoubleMatrixR &,
                      const RTSParams &, WorkerChannel &,
                      std::vector<MatchedPoints> &);

}  // namespace AlignCalc

#endif
