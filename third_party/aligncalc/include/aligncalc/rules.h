#ifndef ALIGNCALC_RULES_H
#define ALIGNCALC_RULES_H
#include "alignTypes.h"
#include <array>
#include <cstdint>
#include <vector>

namespace AlignCalc
{
    // Which columns of a caller's row hold a point's position, and
    // optionally a label (alignPoints: only equal labels match) and an
    // orientation in degrees (the similarity model's oriented-point
    // prefilter, 2-D only); -1 = none. Coordinate d is in column
    // pos[d], or column d past the end of pos.
    struct PointCols
    {
        int dim = 2;
        std::vector<int> pos = {0, 1, 2};
        int label = -1;
        int angle = -1;

        int col(int d) const
        {
            return d < int(pos.size()) ? pos[d] : d;
        }
        bool valid(const DoubleMatrixR& m) const
        {
            if (dim < 1) return false;
            for (int d = 0; d < dim; ++d)
                if (col(d) < 0 || col(d) >= m.cols()) return false;
            return label < m.cols() && angle < m.cols();
        }
    };

    // Rules hooks, called by the models with the caller's rows. Pair
    // hooks describe the two correspondences a = (i1 <-> i2) and
    // b = (j1 <-> j2) as (L, i1, j1, R, i2, j2); they must give the
    // same answer for (L, j1, i1, R, j2, i2). tolScale is clamped to
    // (0, 1], so rules can only tighten a model's tolerance.
    // acceptTransform vets a fitted alignment (PointTransform for
    // points, Eigen::Isometry3d for lines): false sets it aside
    // without reporting it, and the distinct search goes on.
    //
    // LabelRules: a node is admissible iff the label columns are
    // equal (labelCol < 0: no label, everything admissible).
    struct LabelRules
    {
        int labelCol = -1;

        bool admissible(const DoubleMatrixR& L, int32_t l,
                        const DoubleMatrixR& R, int32_t r) const
        {
            return labelCol < 0 || L(l, labelCol) == R(r, labelCol);
        }

        template <class Invariants>
        bool edgeOk(const DoubleMatrixR&, int32_t, int32_t,
                    const DoubleMatrixR&, int32_t, int32_t,
                    const Invariants&) const
        {
            return true;
        }

        double tolScale(const DoubleMatrixR&, int32_t, int32_t,
                        const DoubleMatrixR&, int32_t, int32_t) const
        {
            return 1.0;
        }

        // triangle models: left[v] matches right[v]
        bool tripleOk(const DoubleMatrixR&,
                      const std::array<int32_t, 3>&,
                      const DoubleMatrixR&,
                      const std::array<int32_t, 3>&) const
        {
            return true;
        }

        template <class Transform>
        bool acceptTransform(const Transform&) const
        {
            return true;
        }
    };

    namespace detail
    {
        inline double clampTolScale(double s)
        {
            return (s > 0.0 && s < 1.0) ? s : (s > 0.0 ? 1.0 : 0.0);
        }
    }  // namespace detail

}  // namespace AlignCalc

#endif
