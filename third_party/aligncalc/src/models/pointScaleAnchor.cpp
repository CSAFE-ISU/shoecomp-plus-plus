#include "aligncalc/pointScaleAnchor.h"
#include <algorithm>
#include <cmath>

namespace AlignCalc
{
    namespace
    {
        void distances(const DoubleMatrixR& m, const PointCols& c,
                       std::vector<double>& out)
        {
            const Eigen::Index n = m.rows();
            out.assign(size_t(n) * n, 0.0);
            for (Eigen::Index i = 0; i < n; ++i)
                for (Eigen::Index j = i + 1; j < n; ++j)
                {
                    double s = 0.0;
                    for (int d = 0; d < c.dim; ++d)
                    {
                        const double t =
                            m(i, c.col(d)) - m(j, c.col(d));
                        s += t * t;
                    }
                    out[size_t(i) * n + j] = out[size_t(j) * n + i] =
                        std::sqrt(s);
                }
        }
    }  // namespace

    PointScaleAnchor::PointScaleAnchor(const DoubleMatrixR& left,
                                       const DoubleMatrixR& right,
                                       const PointCols& cols,
                                       double scaleRange, double tol)
        : scaleRange_(scaleRange), tol_(tol)
    {
        if (!cols.valid(left) || !cols.valid(right)) return;
        nL_ = int32_t(left.rows());
        nR_ = int32_t(right.rows());
        distances(left, cols, dL_);
        distances(right, cols, dR_);
    }

    void PointScaleAnchor::filter(Correspondence a, Correspondence b,
                                  const Correspondence* cands, size_t n,
                                  uint8_t* keep) const
    {
        std::fill(keep, keep + n, uint8_t(0));
        if (nL_ == 0 || nR_ == 0) return;
        auto dl = [this](int32_t i, int32_t j)
        { return dL_[size_t(i) * nL_ + j]; };
        auto dr = [this](int32_t i, int32_t j)
        { return dR_[size_t(i) * nR_ + j]; };
        const double base = dr(a.right, b.right);
        const double lab = dl(a.left, b.left);
        if (!(lab > 0.0) || !(base > 0.0)) return;
        const double scale = base / lab;
        const double widen = 1.0 + tol_;
        if (scale > scaleRange_ * widen ||
            scale * scaleRange_ * widen < 1)
            return;
        for (size_t k = 0; k < n; ++k)
        {
            const Correspondence c = cands[k];
            const double ra = dr(c.right, a.right);
            const double rb = dr(c.right, b.right);
            const double lim = tol_ * std::max(base, std::max(ra, rb));
            keep[k] =
                std::fabs(scale * dl(c.left, a.left) - ra) <= lim &&
                std::fabs(scale * dl(c.left, b.left) - rb) <= lim;
        }
    }
}  // namespace AlignCalc
