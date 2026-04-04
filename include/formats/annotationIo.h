#ifndef SHOECOMP_FORMATS_ANNOTATION_IO
#define SHOECOMP_FORMATS_ANNOTATION_IO

#include "jtjson/json.h"
#include <string>

namespace shoecomp
{
    int saveAnnotationsToFile(const std::string& filePath,
                              const jt::Json& annotations);

    int loadAnnotationsFromFile(const std::string& filePath,
                                jt::Json& annotations);
} /* namespace shoecomp */

#endif
