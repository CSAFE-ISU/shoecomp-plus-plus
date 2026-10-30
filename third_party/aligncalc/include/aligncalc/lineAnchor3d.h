#ifndef ALIGNCALC_LINEANCHOR3D_H
#define ALIGNCALC_LINEANCHOR3D_H
#include "correspondence.h"
#include "rigidLines3d.h"
#include <vector>

namespace AlignCalc
{
    // Clique constraint for RigidLines3D (see CliqueConstraint). The
    // anchor pair (a, b) fixes a rigid T from the frame of each side's
    // two lines: mean direction d, the offset p between the midpoints
    // perpendicular to d, and d x p (both signs of the right d are
    // tried, the one keeping more candidates wins; with |oriented|
    // only the matching sign). A candidate c is
    // kept when T maps its left midpoint close to its right line,
    // measured in the right anchor frame: the offset r from the right
    // line has |r . p^| <= acrossTol (across the bundle) and
    // |r . (d x p^)| <= depthTol (depth). The pairwise separation test
    // cannot see depth for lines spread across a land (it enters |p|
    // at second order). An anchor with |p| = 0 keeps nothing.
    //
    // The tolerances are not widened for the anchor's own error: a
    // widening that grows with distance / |p| (roll error) let the
    // search pick its largest clique under the loosest, closest
    // anchor and undid the check (HS10, 2026-10-20).
    //
    // Keeps references to |left| and |right|; they must outlive it.
    class ALIGNCALC_API LineAnchor3D : public CliqueConstraint
    {
       public:
        LineAnchor3D(const DoubleMatrixR& left,
                     const DoubleMatrixR& right, const LineCols& cols,
                     double acrossTol, double depthTol,
                     bool oriented = false);
        void filter(Correspondence a, Correspondence b,
                    const Correspondence* cands, size_t n,
                    uint8_t* keep) const override;

       private:
        std::vector<Eigen::Vector3d> midL_, dirL_, midR_, dirR_;
        double acrossTol_;
        double depthTol_;
        bool oriented_;
    };

}  // namespace AlignCalc

#endif
