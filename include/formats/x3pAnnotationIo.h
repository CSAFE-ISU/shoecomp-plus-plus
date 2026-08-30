#ifndef SHOECOMP_FORMATS_X3P_ANNOTATION_IO_H
#define SHOECOMP_FORMATS_X3P_ANNOTATION_IO_H

#include "calc/cylinderFit.h"
#include "jtjson/json.h"
#include <string>

// JSON I/O for x3p 3D annotations. See .claude/x3p-notes.md.
namespace shoecomp
{
    int saveX3PAnnotations(const std::string& filePath,
                           const jt::Json& annotations);

    // Returns 0 on success; -1 on I/O, parse, or shape error (leaving
    // |annotations| untouched). Tolerates any subset of the
    // points/bounds/peaks/cylinder keys (legacy point-only files load).
    int loadX3PAnnotations(const std::string& filePath,
                           jt::Json& annotations);

    // Serialize / parse the fitted cylinder frame (angles in degrees,
    // lengths/radius in metres). cylinderFromJson returns false on a
    // malformed object.
    jt::Json cylinderToJson(const CylinderFit& fit);
    bool cylinderFromJson(const jt::Json& j, CylinderFit& out);

    // The 5-D peak descriptor as a JSON object (thetaStart/thetaEnd in
    // degrees, sStart/length/dHeight in metres) — the on-disk shape of
    // a peak with no loop. Shared by manual markup and auto-detection.
    jt::Json peak5DToJson(const PeakRegion5D& d);

    // A striation line as a JSON object: { start:[x,y,z],
    // direction:[unit x,y,z], length } — all physical metres. This is
    // the current on-disk shape of a peaks[] element. Shared by manual
    // markup and auto-detection. striationLineFromJson returns false on
    // a malformed object.
    jt::Json striationLineToJson(const StriationLine& s);
    bool striationLineFromJson(const jt::Json& j, StriationLine& out);
} /* namespace shoecomp */

#endif
