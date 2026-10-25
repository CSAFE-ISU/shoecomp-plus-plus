#ifndef ALIGNCALC_POINTSCALEANCHOR_H
#define ALIGNCALC_POINTSCALEANCHOR_H
#include "correspondence.h"
#include "rules.h"
#include <vector>

namespace AlignCalc
{
    // Clique constraint for similarity point matching in any dimension
    // (see CliqueConstraint), a weaker PointAnchor2D: the anchor pair
    // (a, b) fixes only the scale s = |R[a] R[b]| / |L[a] L[b]|, and a
    // candidate c is kept when s |L[a] L[c]| and s |L[b] L[c]| match
    // |R[a] R[c]| and |R[b] R[c]| within tol * (longest side of the
    // right triangle R[a] R[b] R[c]). So every member shares the
    // anchor's scale, which the triangle edges alone do not ensure.
    // An anchor whose scale lies outside [1/scaleRange, scaleRange]
    // (widened by tol) keeps nothing. Distances cannot see rotation:
    // mirror-image matches are kept.
    class ALIGNCALC_API PointScaleAnchor : public CliqueConstraint
    {
       public:
        PointScaleAnchor(const DoubleMatrixR& left,
                         const DoubleMatrixR& right,
                         const PointCols& cols, double scaleRange,
                         double tol);
        void filter(Correspondence a, Correspondence b,
                    const Correspondence* cands, size_t n,
                    uint8_t* keep) const override;

       private:
        int32_t nL_ = 0;
        int32_t nR_ = 0;
        std::vector<double> dL_;  // nL x nL distances
        std::vector<double> dR_;  // nR x nR distances
        double scaleRange_;
        double tol_;
    };

}  // namespace AlignCalc

#endif
