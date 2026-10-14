#include "formats/imageDecode.h"
#include "stb_image.h"

namespace shoecomp
{
    static bool takeRGBA(unsigned char* data, int iw, int ih,
                         std::vector<unsigned char>& rgba, int& w,
                         int& h, std::string& err)
    {
        if (!data)
        {
            const char* why = stbi_failure_reason();
            err = why ? why : "decode failed";
            return false;
        }
        rgba.assign(data, data + (size_t)iw * ih * 4);
        stbi_image_free(data);
        w = iw;
        h = ih;
        return true;
    }

    bool decodeImageFile(const std::string& path,
                         std::vector<unsigned char>& rgba, int& w,
                         int& h, std::string& err)
    {
        int iw = 0, ih = 0, channels = 0;
        unsigned char* data =
            stbi_load(path.c_str(), &iw, &ih, &channels, 4);
        if (!takeRGBA(data, iw, ih, rgba, w, h, err))
        {
            err = path + ": " + err;
            return false;
        }
        return true;
    }

    bool decodeImageMemory(const unsigned char* bytes, size_t size,
                           std::vector<unsigned char>& rgba, int& w,
                           int& h, std::string& err)
    {
        int iw = 0, ih = 0, channels = 0;
        unsigned char* data = stbi_load_from_memory(
            bytes, (int)size, &iw, &ih, &channels, 4);
        return takeRGBA(data, iw, ih, rgba, w, h, err);
    }
}  // namespace shoecomp
