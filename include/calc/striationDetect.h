#ifndef SHOECOMP_CALC_STRIATION_DETECT_H
#define SHOECOMP_CALC_STRIATION_DETECT_H

#include <atomic>
#include <functional>
#include <string>
#include <vector>

#include "calc/cylinderFit.h"
#include "formats/x3p.h"

// Automatic striation detection for bullet-land x3p surfaces. Bullet
// lands are half-slices of a hollow cylinder; striations are ridges and
// grooves running axially along it. Given a fitted cylinder, unroll the
// surface into a (theta, s) lattice of height above the fitted radius
// (dr = r - R) and pull out the connected ridge/groove regions as 5-D
// peak descriptors. All geometry is physical (metres/radians) via the
// fit + toCylindrical; the render/model-space Z exaggeration is never
// used here.
namespace shoecomp
{
    // Thresholds controlling which candidate regions become striations.
    struct StriationParams
    {
        double minThetaDiff = 0.0;  // radians: min angular WIDTH
        double minDr = 0.0;      // metres: |dr| threshold (both signs)
        double minLength = 0.0;  // metres: min axial length
        int maxCount = 20;       // keep the strongest N by |dHeight|
    };

    // Detect striations on |surf| under the fitted cylinder |fit|.
    // Emits one PeakRegion5D per surviving ridge/groove into |out|
    // (cleared first), ranked by |dHeight| descending and capped at
    // |params.maxCount|. dHeight keeps its sign: positive for a ridge
    // (raised above a convex fit), negative for a groove. Returns false
    // with |err| set on a bad fit/surface or if cancelled; an empty
    // result with no error means nothing passed the filters.
    //
    // |cancel| (optional) is polled between lattice rows; if it becomes
    // true the run aborts (returns false, err = "cancelled").
    // |progress| (optional) is called with a fraction in [0,1] and a
    // short label.
    bool detectStriations(
        const CylinderFit& fit, const Surface3D& surf,
        const StriationParams& params, std::vector<PeakRegion5D>& out,
        std::string& err, const std::atomic<bool>* cancel = nullptr,
        const std::function<void(float, const char*)>& progress = {});
}  // namespace shoecomp

#endif
