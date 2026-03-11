#include "formats.h"
#include "hello_imgui/hello_imgui_include_opengl.h"
#include "stb_image.h"

namespace shoecomp
{
    bool loadJpegFromDisk(const std::string& filePath,
                          ImTextureID& outTextureId, int& outWidth,
                          int& outHeight)
    {
        int w = 0, h = 0, channels = 0;
        unsigned char* data =
            stbi_load(filePath.c_str(), &w, &h, &channels, 4);
        if (!data) return false;

        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                        GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, data);

        stbi_image_free(data);

        outTextureId = (ImTextureID)(intptr_t)tex;
        outWidth = w;
        outHeight = h;
        return true;
    }
}  // namespace shoecomp
