#ifndef ALIGNCALC_RIGIDLINES3D_H
#define ALIGNCALC_RIGIDLINES3D_H
#include "graphBuild.h"
#include "rules.h"
#include <cmath>

namespace AlignCalc
{
    // Which columns of a caller's row describe a line segment:
    // midpoint, direction (any length, any sign) and segment length;
    // optionally a label (alignLines3D: only equal labels match; -1 =
    // none).
    struct LineCols
    {
        std::array<int, 3> mid = {0, 1, 2};
        std::array<int, 3> dir = {3, 4, 5};
        int length = 6;
        int label = -1;

        bool valid(const DoubleMatrixR& m) const
        {
            for (int d = 0; d < 3; ++d)
                if (mid[d] < 0 || mid[d] >= m.cols() || dir[d] < 0 ||
                    dir[d] >= m.cols())
                    return false;
            return length >= 0 && length < m.cols() && label < m.cols();
        }
    };

    // Tolerances on pair invariants. angleTol = 0 turns the angle
    // check off. sepTol and anchorTol are in position units (the
    // defaults suit x3p metres: 5 um, 1 um). extentScale > 0 widens
    // a pair's separation tolerance for segments whose detected
    // extents differ: a midpoint shifted by d along a line moves the
    // separation by d * sin(angle / 2), and the shift is at most half
    // the length difference, so the tolerance becomes
    // sepTol + extentScale * sin(angle / 2) * (dLen_i + dLen_j) / 2.
    struct LineParams
    {
        double angleTol = 5.0 * 3.14159265358979323846 / 180.0;  // rad
        double sepTol = 5e-6;
        double extentScale = 0.0;
        // > 0: anchored clique search (LineAnchor3D) with this depth
        // tolerance and sepTol across; 0 = off.
        double anchorTol = 1e-6;
        // Directions are oriented on both sides: the anchor does not
        // also try the frame turned 180 deg about its offset.
        bool oriented = false;
    };

    // Invariants of a line pair, left (i1, j1) and right (i2, j2):
    // the angle between the two lines and their separation.
    struct LinePairInvariants
    {
        double angleLeft;
        double angleRight;
        double sepLeft;
        double sepRight;
    };

    namespace detail
    {
        // Pairwise angle and separation for all line pairs of |m|
        // (n x n, symmetric, zero diagonal). Separation is the offset
        // between the two midpoints perpendicular to the pair's mean
        // direction.
        ALIGNCALC_API bool lineInvariants(const DoubleMatrixR& m,
                                          const LineCols& cols,
                                          std::vector<double>& angle,
                                          std::vector<double>& sep);

        struct SepEntry
        {
            double sep;
            int32_t j;
        };
        // per row i: the other rows sorted by sep[i][j]
        ALIGNCALC_API void sortBySeparation(
            int32_t n, const std::vector<double>& sep,
            std::vector<SepEntry>& out);
    }  // namespace detail

    // Rigid matching of 3-D line segments (e.g. striations), nodes-
    // first: a = (i1 <-> i2) and b = (j1 <-> j2) are consistent when
    // the pair separation agrees within sepTol and, unless angleTol is
    // 0, the pair angle within angleTol (both scaled down by
    // Rules::tolScale). Directions may have either sign.
    //
    // The model keeps references to |left| and |right|; they must
    // outlive it.
    template <class Rules = LabelRules>
    class RigidLines3D
    {
       public:
        using Invariants = LinePairInvariants;

        RigidLines3D(const DoubleMatrixR& left,
                     const DoubleMatrixR& right, const LineCols& cols,
                     const LineParams& params, Rules rules = Rules{})
            : L_(left), R_(right), params_(params),
              rules_(std::move(rules))
        {
            if (!(params_.angleTol >= 0.0) || !(params_.sepTol >= 0.0))
                return;
            if (!detail::lineInvariants(L_, cols, angL_, sepL_) ||
                !detail::lineInvariants(R_, cols, angR_, sepR_))
                return;
            nL_ = int32_t(L_.rows());
            nR_ = int32_t(R_.rows());
            lenL_.resize(nL_);
            lenR_.resize(nR_);
            for (int32_t i = 0; i < nL_; ++i)
                lenL_[i] = std::fabs(L_(i, cols.length));
            for (int32_t i = 0; i < nR_; ++i)
            {
                lenR_[i] = std::fabs(R_(i, cols.length));
                maxLenR_ = std::max(maxLenR_, lenR_[i]);
            }
            detail::sortBySeparation(nR_, sepR_, sortedR_);
            valid_ = true;
        }
        RigidLines3D(DoubleMatrixR&&, const DoubleMatrixR&,
                     const LineCols&, const LineParams&,
                     Rules = Rules{}) = delete;
        RigidLines3D(const DoubleMatrixR&, DoubleMatrixR&&,
                     const LineCols&, const LineParams&,
                     Rules = Rules{}) = delete;
        RigidLines3D(DoubleMatrixR&&, DoubleMatrixR&&, const LineCols&,
                     const LineParams&, Rules = Rules{}) = delete;

        bool valid() const { return valid_; }
        int32_t leftCount() const { return nL_; }
        int32_t rightCount() const { return nR_; }
        bool admissible(int32_t l, int32_t r) const
        {
            return rules_.admissible(L_, l, R_, r);
        }

        void consistentWith(Correspondence a,
                            std::vector<Correspondence>& out) const
        {
            if (!valid_ || nR_ < 2) return;
            const int32_t i1 = a.left;
            const int32_t i2 = a.right;
            const detail::SepEntry* row =
                sortedR_.data() + size_t(i2) * (nR_ - 1);
            const detail::SepEntry* end = row + (nR_ - 1);
            const double slack = 1e-12 + 1e-9 * params_.sepTol;
            const bool extent = params_.extentScale > 0.0;
            const bool angle = params_.angleTol > 0.0;
            const double dI =
                extent ? std::fabs(lenL_[i1] - lenR_[i2]) : 0.0;
            for (int32_t j1 = 0; j1 < nL_; ++j1)
            {
                if (j1 == i1) continue;
                const double sL = sepL_[size_t(i1) * nL_ + j1];
                const double tL = angL_[size_t(i1) * nL_ + j1];
                // window: the largest tolerance any j2 could get
                double window = params_.sepTol;
                if (extent)
                    window +=
                        extentTerm(tL + params_.angleTol,
                                   dI + std::max(lenL_[j1], maxLenR_));
                const detail::SepEntry* it = std::lower_bound(
                    row, end, sL - window - slack,
                    [](const detail::SepEntry& e, double v)
                    { return e.sep < v; });
                for (; it != end && it->sep <= sL + window + slack;
                     ++it)
                {
                    const int32_t j2 = it->j;
                    const double sR = it->sep;
                    const double tR = angR_[size_t(i2) * nR_ + j2];
                    const double k = detail::clampTolScale(
                        rules_.tolScale(L_, i1, j1, R_, i2, j2));
                    double sepTol = params_.sepTol;
                    if (extent)
                        sepTol += extentTerm(
                            std::max(tL, tR),
                            dI + std::fabs(lenL_[j1] - lenR_[j2]));
                    if (std::fabs(sL - sR) <= sepTol * k &&
                        (!angle ||
                         std::fabs(tL - tR) <= params_.angleTol * k) &&
                        rules_.edgeOk(L_, i1, j1, R_, i2, j2,
                                      Invariants{tL, tR, sL, sR}))
                    {
                        out.push_back({j1, j2});
                    }
                }
            }
        }

       private:
        // separation slack for a pair angle and summed length
        // differences
        double extentTerm(double angle, double dLen) const
        {
            return params_.extentScale *
                   std::sin(std::min(angle, 3.14159265358979323846) /
                            2.0) *
                   dLen / 2.0;
        }

        const DoubleMatrixR& L_;
        const DoubleMatrixR& R_;
        LineParams params_;
        Rules rules_;
        bool valid_ = false;
        int32_t nL_ = 0;
        int32_t nR_ = 0;
        std::vector<double> angL_, sepL_, angR_, sepR_;
        std::vector<double> lenL_, lenR_;
        double maxLenR_ = 0.0;
        std::vector<detail::SepEntry> sortedR_;
    };

    ALIGNCALC_EXTERN_TEMPLATE(
        class ALIGNCALC_API RigidLines3D<LabelRules>)
    ALIGNCALC_EXTERN_TEMPLATE(ALIGNCALC_API bool buildNodesFirst(
        const RigidLines3D<LabelRules>&, const BuildParams&, Progress&,
        GraphRows&))
    ALIGNCALC_EXTERN_TEMPLATE(ALIGNCALC_API bool buildGraph(
        const RigidLines3D<LabelRules>&, const BuildParams&, Progress&,
        GraphRows&))

}  // namespace AlignCalc

#endif
