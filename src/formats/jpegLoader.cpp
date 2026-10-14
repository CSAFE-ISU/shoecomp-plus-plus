#include "formats/jpeg.h"
#include "formats/png.h"
#include "formats/imageDecode.h"

namespace shoecomp
{
    bool loadJpegFromDisk(const std::string& filePath,
                          ImTextureID& outTextureId, int& outWidth,
                          int& outHeight)
    {
        std::vector<unsigned char> rgba;
        std::string err;
        if (!decodeImageFile(filePath, rgba, outWidth, outHeight, err))
            return false;
        outTextureId =
            createTextureRGBA(rgba.data(), outWidth, outHeight);
        return true;
    }
}  // namespace shoecomp
