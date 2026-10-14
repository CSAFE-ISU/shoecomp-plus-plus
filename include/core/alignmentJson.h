#ifndef SHOECOMP_CORE_ALIGNMENT_JSON_H
#define SHOECOMP_CORE_ALIGNMENT_JSON_H

#include <vector>

#include "aligncalc/eigen/Eigen/Geometry"
#include "calc/align3d.h"
#include "core/alignment.h"
#include "core/params.h"
#include "jtjson/json.h"

// The "Save Alignment" JSON shapes, shared by the GUI and scpp.
namespace shoecomp
{
    jt::Json vec3fToJson(const Eigen::Vector3f& v);

    // { mode, rotation (rad), dx, dy, scale, info }
    jt::Json alignment2DToJson(const AlignState& a);

    // { mode: "Automatic", matrix[4][4], rotationDeg, translation,
    // info }; |physical| maps left to right, metres.
    jt::Json automatic3DToJson(const Eigen::Isometry3d& physical,
                               const jt::Json& info);

    // The info object of an automatic 3-D alignment: matched peaks[]
    // index pairs, fit rms, axial-weak flag and the tolerances used.
    // |leftPeakIdx|/|rightPeakIdx| map striation rows to peaks[].
    jt::Json striationMatchInfo(const StriationAlignment& r,
                                const std::vector<int>& leftPeakIdx,
                                const std::vector<int>& rightPeakIdx,
                                const Align3DSettings& s);
}  // namespace shoecomp

#endif
