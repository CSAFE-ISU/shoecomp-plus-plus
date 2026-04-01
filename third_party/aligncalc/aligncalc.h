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

    struct RTSParams
    {
        double delta;
        double epsilon;
        size_t numWorkers;
        size_t numResults;
        int32_t lowerBound;
        int32_t upperBound;
    };

    using DoubleMatrixR =
        Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic,
                      Eigen::RowMajor>;

    bool RTSAlignment(const DoubleMatrixR &, const DoubleMatrixR &,
                      const RTSParams &, WorkerChannel &,
                      std::vector<std::unordered_set<int32_t>> &);

}  // namespace AlignCalc

#endif
