#ifndef SHOECOMP_FORMATS_PNG
#define SHOECOMP_FORMATS_PNG

#include "imgui.h"
#include <string>

namespace shoecomp
{
    bool loadPngFromDisk(const std::string& filePath,
                         ImTextureID& outTextureId, int& outWidth,
                         int& outHeight);

    int savePngToDisk(const std::string& filePath,
                      const unsigned char* rgbaData, int width,
                      int height);

    ImTextureID createTextureRGBA(const unsigned char* rgba, int width,
                                  int height);

    void freeTexture(ImTextureID textureId);

    // Read back RGBA pixels from a GPU texture into |out|.
    // |out| must point to at least width*height*4 bytes.
    // Returns true on success.
    bool saveTextureRGBA(ImTextureID textureId, int width, int height,
                         unsigned char* out);
} /* namespace shoecomp */

#endif
