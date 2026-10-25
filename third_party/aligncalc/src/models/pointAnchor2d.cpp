#include "aligncalc/pointAnchor2d.h"
#include <algorithm>
#include <complex>

namespace AlignCalc
{
    PointAnchor2D::PointAnchor2D(const DoubleMatrixR& left,
                                 const DoubleMatrixR& right,
                                 const PointCols& cols,
                                 double scaleRange, double tol)
        : L_(left), R_(right), x_(cols.col(0)), y_(cols.col(1)),
          scaleRange_(scaleRange), tol_(tol)
    {
    }

    void PointAnchor2D::filter(Correspondence a, Correspondence b,
                               const Correspondence* cands, size_t n,
                               uint8_t* keep) const
    {
        using C = std::complex<double>;
        auto at = [this](const DoubleMatrixR& m, int32_t i)
        { return C(m(i, x_), m(i, y_)); };
        std::fill(keep, keep + n, uint8_t(0));
        const C la = at(L_, a.left), ra = at(R_, a.right);
        const C dl = at(L_, b.left) - la;
        const C dr = at(R_, b.right) - ra;
        const double base = std::abs(dr);
        if (!(std::abs(dl) > 0.0) || !(base > 0.0)) return;
        // T(z) = ra + m (z - la): rotation and scale, no reflection
        const C m = dr / dl;
        const double scale = std::abs(m);
        const double widen = 1.0 + tol_;
        if (scale > scaleRange_ * widen ||
            scale * scaleRange_ * widen < 1)
            return;
        // squared lengths throughout: no square roots per candidate
        const C rb = ra + dr;
        const double tol2 = tol_ * tol_;
        const double base2 = std::norm(dr);
        for (size_t k = 0; k < n; ++k)
        {
            const C rc = at(R_, cands[k].right);
            const C p = ra + m * (at(L_, cands[k].left) - la);
            const double ref2 = std::max(
                base2,
                std::max(std::norm(rc - ra), std::norm(rc - rb)));
            keep[k] = std::norm(p - rc) <= tol2 * ref2;
        }
    }
}  // namespace AlignCalc
