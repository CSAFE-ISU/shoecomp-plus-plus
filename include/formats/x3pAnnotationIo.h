#ifndef SHOECOMP_FORMATS_X3P_ANNOTATION_IO_H
#define SHOECOMP_FORMATS_X3P_ANNOTATION_IO_H

#include "jtjson/json.h"
#include <string>

// JSON I/O for x3p 3D annotations. See .claude/x3p-notes.md.
namespace shoecomp
{
    int saveX3PAnnotations(const std::string& filePath,
                           const jt::Json& annotations);

    // Returns 0 on success; -1 on I/O, parse, or shape error (leaving
    // |annotations| untouched).
    int loadX3PAnnotations(const std::string& filePath,
                           jt::Json& annotations);
} /* namespace shoecomp */

#endif
