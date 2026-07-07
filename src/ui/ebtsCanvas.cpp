#include "ui/ebtsCanvas.h"
#include "formats/ebts.h"
#include "formats/png.h"

namespace shoecomp
{
    std::vector<EBTSCanvas::PointType> EBTSCanvas::allowedPointTypes()
        const
    {
        return {PointType::RidgeEnding, PointType::Bifurcation,
                PointType::Core, PointType::Delta, PointType::Other};
    }

    EBTSCanvas::DetectionSpec EBTSCanvas::detectionSpec() const
    {
        DetectionSpec spec;
        // Class index -> point type for a friction-ridge bbox model.
        spec.classToPointType = {
            PointType::RidgeEnding, PointType::Bifurcation,
            PointType::Core, PointType::Delta, PointType::Other};
        return spec;
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
}  // namespace shoecomp
