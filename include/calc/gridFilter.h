#ifndef SHOECOMP_CALC_GRID_FILTER_H
#define SHOECOMP_CALC_GRID_FILTER_H

#include <vector>

// Small filters on row-major (H x W) float height grids, NaN =
// invalid. GUI-free (used by striation detection and the x3p
// renderer).
namespace shoecomp
{
    // Separable NaN-aware box mean of a (H x W) float grid
    // (row-major index r*W+c), half-window |hw| cells, into |dst|.
    // Cells with no finite neighbour stay NaN.
    void boxMeanSeparable(const std::vector<float>& src, int W, int H,
                          int hw, std::vector<float>& dst);
}  // namespace shoecomp

#endif
