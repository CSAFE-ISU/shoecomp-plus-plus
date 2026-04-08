#include "formats/png.h"
#include "stb_image.h"

namespace shoecomp
{
    bool loadPngFromDisk(const std::string& filePath,
                         ImTextureID& outTextureId, int& outWidth,
                         int& outHeight)
    {
        int w = 0, h = 0, channels = 0;
        unsigned char* data =
            stbi_load(filePath.c_str(), &w, &h, &channels, 4);
        if (!data) return false;

        outTextureId = createTextureRGBA(data, w, h);

        stbi_image_free(data);

        outWidth = w;
        outHeight = h;
        return true;
    }
}  // namespace shoecomp
