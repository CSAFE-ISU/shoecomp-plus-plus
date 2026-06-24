#include "ui/imageCanvasKinds.h"
#include "formats/png.h"
#include "formats/jpeg.h"
#include "formats/ebts.h"

namespace shoecomp
{
    const std::vector<std::string>& ShoeCanvas::imageExtensions() const
    {
        static const std::vector<std::string> exts = {
            ".png", ".PNG", ".jpg", ".JPG", ".jpeg", ".JPEG"};
        return exts;
    }

    int ShoeCanvas::loadImages(
        const std::string& path,
        std::vector<std::unique_ptr<ImageCanvas>>& out,
        std::string& err) const
    {
        std::string ext = lowerExt(path);
        ImTextureID tex = 0;
        int w = 0, h = 0;
        bool ok = false;
        if (ext == ".png")
            ok = loadPngFromDisk(path, tex, w, h);
        else if (ext == ".jpg" || ext == ".jpeg")
            ok = loadJpegFromDisk(path, tex, w, h);
        else
        {
            err = "ShoeCanvas supports PNG and JPEG only:\n" + path;
            return -1;
        }
        if (!ok)
        {
            err = "Failed to load image from:\n" + path;
            return -1;
        }
        auto c = std::make_unique<ShoeCanvas>();
        c->fillRaster(path, tex, w, h);
        out.push_back(std::move(c));
        return 1;
    }

    const std::vector<std::string>& EBTSCanvas::imageExtensions() const
    {
        static const std::vector<std::string> exts = {
            ".png", ".PNG", ".an2", ".AN2", ".irr", ".lffs", ".ebts"};
        return exts;
    }

    int EBTSCanvas::loadImages(
        const std::string& path,
        std::vector<std::unique_ptr<ImageCanvas>>& out,
        std::string& err) const
    {
        std::string ext = lowerExt(path);
        if (ext == ".an2" || ext == ".irr" || ext == ".lffs" ||
            ext == ".ebts")
        {
            int n = loadNistFromDisk(
                path, out,
                [] { return std::make_unique<EBTSCanvas>(); });
            if (n <= 0)
            {
                err = "Failed to load NIST/EBTS file:\n" + path;
                return -1;
            }
            return n;
        }
        if (ext == ".png")
        {
            ImTextureID tex = 0;
            int w = 0, h = 0;
            if (!loadPngFromDisk(path, tex, w, h))
            {
                err = "Failed to load image from:\n" + path;
                return -1;
            }
            auto c = std::make_unique<EBTSCanvas>();
            c->fillRaster(path, tex, w, h);
            out.push_back(std::move(c));
            return 1;
        }
        err = "EBTSCanvas supports PNG and EBTS/NIST formats only:\n" +
              path;
        return -1;
    }

    std::unique_ptr<ImageCanvas2D> makeCanvas(ImageCanvas::Kind kind)
    {
        switch (kind)
        {
            case ImageCanvas::Kind::ShoeCanvas:
                return std::make_unique<ShoeCanvas>();
            case ImageCanvas::Kind::EBTSCanvas:
                return std::make_unique<EBTSCanvas>();
            case ImageCanvas::Kind::Canvas2D:
            default:
                return std::make_unique<ImageCanvas2D>();
        }
    }
}  // namespace shoecomp
