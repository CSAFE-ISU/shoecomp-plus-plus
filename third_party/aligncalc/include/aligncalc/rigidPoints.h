#ifndef ALIGNCALC_RIGIDPOINTS_H
#define ALIGNCALC_RIGIDPOINTS_H
#include "graphBuild.h"
#include "rules.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace AlignCalc
{
    struct PointPairInvariants
    {
        double dLeft;   // d_L(i1, j1)
        double dRight;  // d_R(i2, j2)
    };

    // Distance-preserving (rigid) point matching in 1-3 dimensions:
    // a = (i1 <-> i2) and b = (j1 <-> j2) are consistent when
    // |d_L(i1, j1) - d_R(i2, j2)| <= epsilon * d_R(i2, j2), with
    // epsilon scaled down by Rules::tolScale. Nodes-first.
    //
    // The model keeps references to |left| and |right|; they must
    // outlive it.
    template <class Rules = LabelRules>
    class RigidPoints
    {
       public:
        using Invariants = PointPairInvariants;

        RigidPoints(const DoubleMatrixR& left,
                    const DoubleMatrixR& right, const PointCols& cols,
                    double epsilon, Rules rules = Rules{});
        RigidPoints(DoubleMatrixR&&, const DoubleMatrixR&,
                    const PointCols&, double, Rules = Rules{}) = delete;
        RigidPoints(const DoubleMatrixR&, DoubleMatrixR&&,
                    const PointCols&, double, Rules = Rules{}) = delete;
        RigidPoints(DoubleMatrixR&&, DoubleMatrixR&&, const PointCols&,
                    double, Rules = Rules{}) = delete;

        bool valid() const { return valid_; }
        int32_t leftCount() const { return nL_; }
        int32_t rightCount() const { return nR_; }
        bool admissible(int32_t l, int32_t r) const
        {
            return rules_.admissible(L_, l, R_, r);
        }
        void consistentWith(Correspondence a,
                            std::vector<Correspondence>& out) const;

       private:
        struct Entry
        {
            double d;
            int32_t j;
        };

        static double dist(const DoubleMatrixR& m, const PointCols& c,
                           int32_t i, int32_t j)
        {
            double s = 0.0;
            for (int d = 0; d < c.dim; ++d)
            {
                double t = m(i, c.pos[d]) - m(j, c.pos[d]);
                s += t * t;
            }
            return std::sqrt(s);
        }

        const DoubleMatrixR& L_;
        const DoubleMatrixR& R_;
        PointCols cols_;
        double eps_;
        Rules rules_;
        bool valid_ = false;
        int32_t nL_ = 0;
        int32_t nR_ = 0;
        std::vector<double> dL_;      // nL x nL
        std::vector<Entry> sortedR_;  // per right point: others by d
    };

    template <class Rules>
    RigidPoints<Rules>::RigidPoints(const DoubleMatrixR& left,
                                    const DoubleMatrixR& right,
                                    const PointCols& cols,
                                    double epsilon, Rules rules)
        : L_(left), R_(right), cols_(cols), eps_(epsilon),
          rules_(std::move(rules))
    {
        if (!cols_.valid(L_) || !cols_.valid(R_) || !(eps_ >= 0.0))
            return;
        valid_ = true;
        nL_ = static_cast<int32_t>(L_.rows());
        nR_ = static_cast<int32_t>(R_.rows());

        dL_.resize(size_t(nL_) * nL_);
        for (int32_t i = 0; i < nL_; ++i)
            for (int32_t j = 0; j < nL_; ++j)
                dL_[size_t(i) * nL_ + j] =
                    i == j ? 0.0 : dist(L_, cols_, i, j);

        if (nR_ < 2) return;
        sortedR_.resize(size_t(nR_) * (nR_ - 1));
        for (int32_t i = 0; i < nR_; ++i)
        {
            Entry* row = sortedR_.data() + size_t(i) * (nR_ - 1);
            int32_t k = 0;
            for (int32_t j = 0; j < nR_; ++j)
                if (j != i) row[k++] = {dist(R_, cols_, i, j), j};
            std::sort(row, row + k,
                      [](const Entry& a, const Entry& b)
                      { return a.d != b.d ? a.d < b.d : a.j < b.j; });
        }
    }

    template <class Rules>
    void RigidPoints<Rules>::consistentWith(
        Correspondence a, std::vector<Correspondence>& out) const
    {
        if (!valid_ || nR_ < 2) return;
        const int32_t i1 = a.left;
        const int32_t i2 = a.right;
        const Entry* row = sortedR_.data() + size_t(i2) * (nR_ - 1);
        const Entry* end = row + (nR_ - 1);
        // widened bounds; the exact test below decides
        const double loF = (1.0 / (1.0 + eps_)) * (1.0 - 1e-9);
        const double hiF =
            eps_ < 1.0 ? (1.0 / (1.0 - eps_)) * (1.0 + 1e-9)
                       : std::numeric_limits<double>::infinity();
        for (int32_t j1 = 0; j1 < nL_; ++j1)
        {
            if (j1 == i1) continue;
            const double d1 = dL_[size_t(i1) * nL_ + j1];
            const double lo = d1 * loF;
            const double hi = d1 * hiF;
            const Entry* it = std::lower_bound(
                row, end, lo,
                [](const Entry& e, double v) { return e.d < v; });
            for (; it != end && it->d <= hi; ++it)
            {
                const int32_t j2 = it->j;
                const double d2 = it->d;
                const double e =
                    eps_ * detail::clampTolScale(
                               rules_.tolScale(L_, i1, j1, R_, i2, j2));
                if (d1 >= d2 * (1 - e) && d1 <= d2 * (1 + e) &&
                    rules_.edgeOk(L_, i1, j1, R_, i2, j2,
                                  Invariants{d1, d2}))
                {
                    out.push_back({j1, j2});
                }
            }
        }
    }

    extern template class RigidPoints<LabelRules>;
    extern template bool buildNodesFirst(const RigidPoints<LabelRules>&,
                                         const BuildParams&, Progress&,
                                         GraphRows&);
    extern template bool buildGraph(const RigidPoints<LabelRules>&,
                                    const BuildParams&, Progress&,
                                    GraphRows&);

}  // namespace AlignCalc

#endif
