#ifndef SHOECOMP_FORMATS_PNG
#define SHOECOMP_FORMATS_PNG

#include "imgui.h"
#include <string>

namespace shoecomp
{
    bool loadPngFromDisk(
        const std::string& filePath,
        ImTextureID& outTextureId,
        int& outWidth, int& outHeight);

    int savePngToDisk(
        const std::string& filePath,
        const unsigned char* rgbaData,
        int width, int height);

    void freeTexture(ImTextureID textureId);
} /* namespace shoecomp */

#endif
