#ifndef SHOECOMP_FORMATS
#define SHOECOMP_FORMATS

#include "imgui.h"
#include "json.h"
#include <string>

namespace shoecomp
{
    bool loadPngFromDisk(const std::string& filePath,
                         ImTextureID& outTextureId, int& outWidth,
                         int& outHeight);
    bool loadJpegFromDisk(const std::string& filePath,
                          ImTextureID& outTextureId, int& outWidth,
                          int& outHeight);
    void freeTexture(ImTextureID textureId);

    int saveAnnotationsToFile(const std::string& filePath,
                              const jt::Json& annotations);
    int loadAnnotationsFromFile(const std::string& filePath,
                                jt::Json& annotations);
} /* namespace shoecomp */

#endif
