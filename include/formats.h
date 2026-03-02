#ifndef SHOECOMP_FORMATS
#define SHOECOMP_FORMATS

#include "imgui.h"
#include <string>

namespace shoecomp
{
    bool loadPngFromDisk(const std::string& filePath,
                         ImTextureID& outTextureId,
                         int& outWidth,
                         int& outHeight);
    void freeTexture(ImTextureID textureId);
} /* namespace shoecomp */

#endif
