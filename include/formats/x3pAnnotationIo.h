#ifndef SHOECOMP_FORMATS_X3P_ANNOTATION_IO_H
#define SHOECOMP_FORMATS_X3P_ANNOTATION_IO_H

#include "jtjson/json.h"
#include <string>

// JSON persistence for x3p (3D surface) annotations. Unlike the 2D
// annotationIo, each element carries BOTH physical coordinates in
// metres {x, y, z} AND grid indices {col, row}. The grid index is the
// robust re-load anchor (independent of the surface's
// increment/offset), while the physical coordinates are informative for
// external tools working in the x3p coordinate system. Points
// additionally carry a "type" string.
namespace shoecomp
{
    // Serialize |annotations| ({"points":[...], "bounds":[...]}) to
    // |filePath|. Returns 0 on success, -1 on I/O failure.
    int saveX3PAnnotations(const std::string& filePath,
                           const jt::Json& annotations);

    // Parse |filePath| into |annotations|, validating that every point
    // has numeric col/row/x/y/z (+ a string type) and every bound has
    // numeric col/row/x/y/z. Returns 0 on success, -1 on I/O, parse, or
    // shape errors (in which case |annotations| is left untouched).
    int loadX3PAnnotations(const std::string& filePath,
                           jt::Json& annotations);
} /* namespace shoecomp */

#endif
