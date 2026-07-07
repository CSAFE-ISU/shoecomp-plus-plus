#include "ui/shoeCanvas.h"
#include "formats/jpeg.h"
#include "formats/png.h"

namespace shoecomp
{
    std::vector<ShoeCanvas::PointType> ShoeCanvas::allowedPointTypes()
        const
    {
        return {PointType::Corner, PointType::Center};
    }

    ShoeCanvas::DetectionSpec ShoeCanvas::detectionSpec() const
    {
        DetectionSpec spec;
        // Class index -> point type for a shoeprint bbox model.
        spec.classToPointType = {PointType::Corner, PointType::Center};
        return spec;
    }

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
}  // namespace shoecomp
