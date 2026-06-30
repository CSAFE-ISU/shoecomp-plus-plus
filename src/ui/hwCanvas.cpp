#include "ui/hwCanvas.h"
#include "formats/jpeg.h"
#include "formats/png.h"

namespace shoecomp
{
    std::vector<HWCanvas::PointType> HWCanvas::allowedPointTypes() const
    {
        return {PointType::WordStart, PointType::WordEnd,
                PointType::Intersection, PointType::CurveTurn};
    }

    const std::vector<std::string>& HWCanvas::imageExtensions() const
    {
        static const std::vector<std::string> exts = {
            ".png", ".PNG", ".jpg", ".JPG", ".jpeg", ".JPEG"};
        return exts;
    }

    int HWCanvas::loadImages(
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
            err = "HWCanvas supports PNG and JPEG only:\n" + path;
            return -1;
        }
        if (!ok)
        {
            err = "Failed to load image from:\n" + path;
            return -1;
        }
        auto c = std::make_unique<HWCanvas>();
        c->fillRaster(path, tex, w, h);
        out.push_back(std::move(c));
        return 1;
    }
}  // namespace shoecomp
