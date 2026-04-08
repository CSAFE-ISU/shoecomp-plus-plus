#include "formats/png.h"
#include "hello_imgui/hello_imgui_include_opengl.h"

namespace shoecomp
{
    ImTextureID createTextureRGBA(const unsigned char* rgba, int width,
                                  int height)
    {
        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                        GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        return (ImTextureID)(intptr_t)tex;
    }

    void freeTexture(ImTextureID textureId)
    {
        GLuint tex = (GLuint)(intptr_t)textureId;
        if (tex != 0) glDeleteTextures(1, &tex);
    }
}  // namespace shoecomp
