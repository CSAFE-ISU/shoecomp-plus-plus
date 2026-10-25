#ifndef ALIGNCALC_POINTANCHOR2D_H
#define ALIGNCALC_POINTANCHOR2D_H
#include "correspondence.h"
#include "rules.h"

namespace AlignCalc
{
    // Clique constraint for 2-D point matching (see CliqueConstraint),
    // for SimilarityPoints and, with scaleRange 1, RigidPoints. The
    // anchor pair (a, b) fixes a similarity T without reflection,
    // T(L[a]) = R[a] and T(L[b]) = R[b]; a candidate c is kept when
    // |T(L[c]) - R[c]| <= tol * (longest side of the right triangle
    // R[a] R[b] R[c]). An anchor whose scale lies outside
    // [1/scaleRange, scaleRange] (widened by tol) keeps nothing, so
    // scaleRange 1 gives a rigid anchor. Without reflection, a
    // mirror-image match keeps only points near its anchor line.
    //
    // Keeps references to |left| and |right|; they must outlive it.
    class ALIGNCALC_API PointAnchor2D : public CliqueConstraint
    {
       public:
        PointAnchor2D(const DoubleMatrixR& left,
                      const DoubleMatrixR& right, const PointCols& cols,
                      double scaleRange, double tol);
        void filter(Correspondence a, Correspondence b,
                    const Correspondence* cands, size_t n,
                    uint8_t* keep) const override;

       private:
        const DoubleMatrixR& L_;
        const DoubleMatrixR& R_;
        int x_;
        int y_;
        double scaleRange_;
        double tol_;
    };

}  // namespace AlignCalc

#endif
