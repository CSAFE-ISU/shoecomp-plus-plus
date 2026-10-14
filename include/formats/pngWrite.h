#ifndef SHOECOMP_FORMATS_PNG_WRITE
#define SHOECOMP_FORMATS_PNG_WRITE

#include <string>

namespace shoecomp
{
    // Write 8-bit RGBA pixels (row-major, top-left origin) as a PNG.
    // Returns 0 on success, -1 on failure.
    int savePngToDisk(const std::string& filePath,
                      const unsigned char* rgbaData, int width,
                      int height);
} /* namespace shoecomp */

#endif
