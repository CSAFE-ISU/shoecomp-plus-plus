#include "formats/png.h"
#include "hello_imgui/hello_imgui.h"

#if defined(HELLOIMGUI_HAS_OPENGL)
#include "hello_imgui/hello_imgui_include_opengl.h"
#elif defined(HELLOIMGUI_HAS_METAL)
#include "hello_imgui/internal/backend_impls/rendering_metal.h"
#import <Metal/Metal.h>
#elif defined(HELLOIMGUI_HAS_DIRECTX11)
#include "hello_imgui/internal/backend_impls/rendering_dx11.h"
#include <d3d11.h>
#endif

namespace shoecomp
{
#if defined(HELLOIMGUI_HAS_OPENGL)
    // ---------------------------------------------------------------
    // OpenGL backend (Linux default, and Windows/macOS when selected)
    // ---------------------------------------------------------------
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

    bool saveTextureRGBA(ImTextureID textureId, int width, int height,
                         unsigned char* out)
    {
#ifndef __EMSCRIPTEN__
        GLuint tex = (GLuint)(intptr_t)textureId;
        if (tex == 0) return false;
        glBindTexture(GL_TEXTURE_2D, tex);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, out);
        return true;
#else
        return false; /* TODO: do we need saving on web? */
#endif
    }

#elif defined(HELLOIMGUI_HAS_METAL)
    // ---------------------------------------------------------------
    // Metal backend (macOS)
    // ---------------------------------------------------------------
    ImTextureID createTextureRGBA(const unsigned char* rgba, int width,
                                  int height)
    {
        id<MTLDevice> device =
            HelloImGui::GetMetalGlobals().caMetalLayer.device;
        MTLTextureDescriptor* desc = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                         width:width
                                        height:height
                                     mipmapped:NO];
        desc.usage = MTLTextureUsageShaderRead;
        id<MTLTexture> tex = [device newTextureWithDescriptor:desc];
        [tex replaceRegion:MTLRegionMake2D(0, 0, width, height)
               mipmapLevel:0
                 withBytes:rgba
               bytesPerRow:(NSUInteger)(width * 4)];
        return (ImTextureID)(__bridge_retained void*)tex;
    }

    void freeTexture(ImTextureID textureId)
    {
        if (textureId == 0) return;
        id<MTLTexture> tex =
            (__bridge_transfer id<MTLTexture>)(void*)textureId;
        (void)tex;  // ARC releases at end of scope
    }

    bool saveTextureRGBA(ImTextureID textureId, int width, int height,
                         unsigned char* out)
    {
        if (textureId == 0) return false;
        id<MTLTexture> tex = (__bridge id<MTLTexture>)(void*)textureId;
        [tex getBytes:out
            bytesPerRow:(NSUInteger)(width * 4)
             fromRegion:MTLRegionMake2D(0, 0, width, height)
            mipmapLevel:0];
        return true;
    }

#elif defined(HELLOIMGUI_HAS_DIRECTX11)
    // ---------------------------------------------------------------
    // DirectX 11 backend (Windows)
    // ---------------------------------------------------------------
    ImTextureID createTextureRGBA(const unsigned char* rgba, int width,
                                  int height)
    {
        ID3D11Device* device = HelloImGui::GetDx11Globals().pd3dDevice;
        if (!device) return 0;

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = (UINT)width;
        desc.Height = (UINT)height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA init = {};
        init.pSysMem = rgba;
        init.SysMemPitch = (UINT)(width * 4);

        ID3D11Texture2D* tex = nullptr;
        if (FAILED(device->CreateTexture2D(&desc, &init, &tex)))
            return 0;

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        ID3D11ShaderResourceView* srv = nullptr;
        device->CreateShaderResourceView(tex, &srvDesc, &srv);
        tex->Release();
        return (ImTextureID)srv;
    }

    void freeTexture(ImTextureID textureId)
    {
        if (textureId == 0) return;
        ID3D11ShaderResourceView* srv =
            (ID3D11ShaderResourceView*)textureId;
        srv->Release();
    }

    bool saveTextureRGBA(ImTextureID textureId, int width, int height,
                         unsigned char* out)
    {
        if (textureId == 0) return false;
        ID3D11ShaderResourceView* srv =
            (ID3D11ShaderResourceView*)textureId;

        ID3D11Resource* res = nullptr;
        srv->GetResource(&res);
        if (!res) return false;

        ID3D11Texture2D* tex = nullptr;
        res->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&tex);
        res->Release();
        if (!tex) return false;

        ID3D11Device* device = nullptr;
        tex->GetDevice(&device);
        ID3D11DeviceContext* ctx = nullptr;
        device->GetImmediateContext(&ctx);

        D3D11_TEXTURE2D_DESC desc = {};
        tex->GetDesc(&desc);
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags = 0;

        ID3D11Texture2D* staging = nullptr;
        if (FAILED(device->CreateTexture2D(&desc, nullptr, &staging)))
        {
            ctx->Release();
            device->Release();
            tex->Release();
            return false;
        }

        ctx->CopyResource(staging, tex);

        D3D11_MAPPED_SUBRESOURCE mapped = {};
        bool ok = false;
        if (SUCCEEDED(ctx->Map(staging, 0, D3D11_MAP_READ, 0, &mapped)))
        {
            const unsigned char* src =
                (const unsigned char*)mapped.pData;
            for (int y = 0; y < height; ++y)
                memcpy(out + y * width * 4, src + y * mapped.RowPitch,
                       width * 4);
            ctx->Unmap(staging, 0);
            ok = true;
        }

        staging->Release();
        ctx->Release();
        device->Release();
        tex->Release();
        return ok;
    }

#else
#error "No supported hello_imgui rendering backend defined"
#endif
}  // namespace shoecomp
