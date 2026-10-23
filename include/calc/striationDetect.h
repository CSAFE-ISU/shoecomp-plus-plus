#ifndef SHOECOMP_CALC_STRIATION_DETECT_H
#define SHOECOMP_CALC_STRIATION_DETECT_H

#include <atomic>
#include <functional>
#include <string>
#include <vector>

#include "calc/cylinderFit.h"
#include "formats/x3p.h"

// Automatic striation detection for bullet-land x3p surfaces.
// Striations are near-parallel ridges/grooves running axially along the
// land; a striation that runs axially on a cylinder is a straight 3-D
// line (a ruling). We detect them directly on the raw (x,y,z) surface
// as straight line segments via a 3-D Hough transform, WITHOUT a
// cylinder fit (real lands are not perfect cylinders, so a global fit
// would distort the height feature). Feature points are local
// ridge/groove crests found from a local high-pass of the height grid,
// so the prominence measure is fit-independent. The cylinder fit, if
// any, is used only at display time to derive angular/dr/axial-length
// labels.
namespace shoecomp
{
    // Thresholds controlling which candidate lines become striations.
    struct StriationParams
    {
        double minLength = 0.0;      // metres: min striation length
        double minHeight = 0.0;      // metres: min local ridge/groove
                                     // prominence (|height difference|)
        double maxSurfaceDev = 0.0;  // radians: max angle of the line
                                     // direction out of the local
                                     // surface tangent plane (0 = off)
        int maxCount = 20;           // keep the strongest N
        bool includeValleys = true;  // true: detect ridges (peaks) and
                                     // grooves (valleys); false: only
                                     // ridges (positive local height)
        bool excludeShoulders = true;  // skip the groove shoulders
                                       // (found from the land's
                                       // cross-section profile)
        double shoulderMargin = 0.0;   // metres: also skip this much
                                       // inside each found shoulder
        // Hough direction search: a grid of candidate directions around
        // the estimated axial direction, -cone to +cone in steps, along
        // the surface (across the land) and out of it (depth); radians.
        // A line cannot settle much off a grid direction, so the step
        // bounds how well tilted striations are followed.
        double dirCone = 12.0 * 3.14159265358979323846 / 180.0;
        double dirStep = 2.5 * 3.14159265358979323846 / 180.0;
        double dirDepthCone = 12.0 * 3.14159265358979323846 / 180.0;
        double dirDepthStep = 2.5 * 3.14159265358979323846 / 180.0;
        int numThreads = 0;  // 0 = hardware concurrency
    };

    // Detect striations on |surf| and emit one StriationLine per
    // surviving ridge/groove into |out| (cleared first), ranked by
    // inlier (crest vote) count with the near-edge striations
    // de-weighted (descending), and capped at |params.maxCount|. The
    // stored geometry is physical metres (start point, unit direction,
    // length); ridge/groove sign and cylinder-relative quantities are
    // recovered at display time. Returns false with |err| set on a bad
    // surface or if cancelled; an empty result with no error means
    // nothing passed the filters.
    //
    // |cancel| (optional) is polled during the scan; if it becomes true
    // the run aborts (returns false, err = "cancelled"). |progress|
    // (optional) is called with a fraction in [0,1] and a short label.
    bool detectStriations(
        const Surface3D& surf, const StriationParams& params,
        std::vector<StriationLine>& out, std::string& err,
        const std::atomic<bool>* cancel = nullptr,
        const std::function<void(float, const char*)>& progress = {});
}  // namespace shoecomp

#endif
