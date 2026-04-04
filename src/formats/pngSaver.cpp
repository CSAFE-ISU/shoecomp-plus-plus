#include "formats/png.h"
#include <png.h>
#include <cstdio>
#include <vector>

namespace shoecomp
{
    int savePngToDisk(const std::string& filePath,
                      const unsigned char* rgbaData, int width,
                      int height)
    {
        if (!rgbaData || width <= 0 || height <= 0) return -1;

        FILE* fp = fopen(filePath.c_str(), "wb");
        if (!fp) return -1;

        png_structp png = png_create_write_struct(
            PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        if (!png)
        {
            fclose(fp);
            return -1;
        }

        png_infop info = png_create_info_struct(png);
        if (!info)
        {
            png_destroy_write_struct(&png, nullptr);
            fclose(fp);
            return -1;
        }

        if (setjmp(png_jmpbuf(png)))
        {
            png_destroy_write_struct(&png, &info);
            fclose(fp);
            return -1;
        }

        png_init_io(png, fp);

        png_set_IHDR(png, info, width, height, 8, PNG_COLOR_TYPE_RGBA,
                     PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                     PNG_FILTER_TYPE_DEFAULT);

        png_write_info(png, info);

        std::vector<const unsigned char*> rows(height);
        int stride = width * 4;
        for (int y = 0; y < height; ++y)
            rows[y] = rgbaData + y * stride;

        png_write_image(png, const_cast<png_bytepp>(rows.data()));
        png_write_end(png, nullptr);

        png_destroy_write_struct(&png, &info);
        fclose(fp);
        return 0;
    }
}  // namespace shoecomp
