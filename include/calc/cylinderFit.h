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

    // 5-D descriptor of a peak region (physical metres + radians):
    // axial start, angular span on the face, height above the fitted
    // curvature, axial length.
    struct PeakRegion5D
    {
        double sStart = 0.0;
        double thetaStart = 0.0;
        double thetaEnd = 0.0;
        double dHeight = 0.0;
        double length = 0.0;
    };

    // Reduce a peak region (a closed loop of grid cells, given as
    // (col,row) pairs) to its 5-D descriptor under |fit|: iterate the
    // finite grid cells inside the loop polygon, convert each to
    // cylindrical coords, and aggregate. sStart/length span the axial
    // extent, thetaStart/thetaEnd the angular extent, and dHeight is
    // the max signed (r - R) — positive for a ridge raised above a
    // convex fit. Returns false if the loop is degenerate or encloses
    // no valid cell.
    bool peakRegion5D(
        const CylinderFit& fit, const Surface3D& surf,
        const std::vector<std::pair<int, int>>& loopColRow,
        PeakRegion5D& out);
}  // namespace shoecomp

#endif
