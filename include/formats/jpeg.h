#ifndef SHOECOMP_FORMATS_JPEG
#define SHOECOMP_FORMATS_JPEG

#include "imgui.h"
#include <string>

namespace shoecomp
{
    bool loadJpegFromDisk(
        const std::string& filePath,
        ImTextureID& outTextureId,
        int& outWidth, int& outHeight);
} /* namespace shoecomp */

#endif
