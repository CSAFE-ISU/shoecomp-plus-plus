#ifndef ALIGNCALC_ALIGNTYPES_H
#define ALIGNCALC_ALIGNTYPES_H
#include "Eigen/Core"
#include <cstddef>

namespace AlignCalc
{
    static constexpr size_t MAX_POINTS = 256;
    using DoubleMatrixR =
        Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic,
                      Eigen::RowMajor>;
}  // namespace AlignCalc

#endif
