#ifndef SHOECOMP_CALC_CYLINDER_FIT_H
#define SHOECOMP_CALC_CYLINDER_FIT_H

#include <string>
#include <utility>
#include <vector>

#include "aligncalc/eigen/Eigen/Core"
#include "aligncalc/eigen/Eigen/Geometry"
#include "formats/x3p.h"

// Fit a cylinder to an x3p surface (bullet lands are half-slices of a
// hollow cylinder) in PHYSICAL metres, and express surface points in
// the resulting cylindrical frame. All geometry here is
// physical/honest; the render/model-space Z exaggeration must not be
// used for fitting.
namespace shoecomp
{
    // Cylinder fitted to a Surface3D, physical metres. curvature = 1/R.
    // The cross-section frame is (e1, e2), both unit and perpendicular
    // to axis; angle theta is measured from e1 (via e2), relative to
    // thetaOrigin so the arc is centered near 0. Axial coordinate s
    // runs along axis. All angles are radians.
    struct CylinderFit
    {
        bool valid = false;
        Eigen::Vector3d axis = Eigen::Vector3d::UnitY();
        Eigen::Vector3d center = Eigen::Vector3d::Zero();
        double radius = 0.0;
        Eigen::Vector3d e1 = Eigen::Vector3d::UnitX();
        Eigen::Vector3d e2 = Eigen::Vector3d::UnitZ();
        double thetaOrigin = 0.0;
        double sMin = 0.0, sMax = 0.0;          // data axial extent
        double thetaMin = 0.0, thetaMax = 0.0;  // data angular extent
    };

    // Fit a cylinder to the finite samples of |surf|. Returns false
    // with |err| set if the surface is too small/degenerate or not
    // plausibly cylindrical. On success |out.valid| is true.
    bool fitCylinder(const Surface3D& surf, CylinderFit& out,
                     std::string& err);

    // Cylindrical coordinates of a physical point under |fit|: angle
    // (radians, relative to thetaOrigin), axial position s, radius r.
    // The height above the fitted surface is (r - fit.radius).
    void toCylindrical(const CylinderFit& fit, const Eigen::Vector3d& p,
                       double& theta, double& s, double& r);

    // Inverse of toCylindrical: the physical point at (theta, s, r).
    Eigen::Vector3d fromCylindrical(const CylinderFit& fit,
                                    double theta, double s, double r);

    // A striation as a straight 3-D line segment in physical metres:
    // one endpoint |start|, unit direction |dir|, and |length| (so
    // end = start + length*dir). This is the raw geometry produced by
    // detection and manual markup; the angular location, dr height, and
    // axial length are DERIVED from a CylinderFit at display time and
    // are deliberately not stored here.
    struct StriationLine
    {
        Eigen::Vector3d start = Eigen::Vector3d::Zero();
        Eigen::Vector3d dir = Eigen::Vector3d::UnitX();
        double length = 0.0;
        bool isPeak = true;  // true = ridge (peak, +dr),
                             // false = groove (valley, -dr)
    };
}  // namespace shoecomp

#endif
