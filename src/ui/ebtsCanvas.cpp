#include "ui/ebtsCanvas.h"
#include "formats/ebts.h"
#include "formats/imageDecode.h"
#include "formats/png.h"

namespace shoecomp
{
    std::vector<EBTSCanvas::PointType> EBTSCanvas::allowedPointTypes()
        const
    {
        return pointTypesFor(ImageType::EBTS);
    }

    EBTSCanvas::DetectionSpec EBTSCanvas::detectionSpec() const
    {
        return detectionSpecFor(ImageType::EBTS);
    }

    const std::vector<std::string>& EBTSCanvas::imageExtensions() const
    {
        return extensionsFor(ImageType::EBTS);
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
            std::vector<EBTSImage> records;
            int n = 0;
            if (parseEBTS(path, records, err) > 0)
            {
                for (auto& r : records)
                {
                    std::vector<unsigned char> rgba;
                    int w = 0, h = 0;
                    std::string decodeErr;
                    if (!decodeImageMemory(r.encoded.data(),
                                           r.encoded.size(), rgba, w, h,
                                           decodeErr))
                        continue;
                    auto c = std::make_unique<EBTSCanvas>();
                    c->image->name = r.name;
                    c->image->path = path;
                    c->image->textureId =
                        createTextureRGBA(rgba.data(), w, h);
                    c->image->width = w;
                    c->image->height = h;
                    c->image->annotations = std::move(r.annotations);
                    out.push_back(std::move(c));
                    ++n;
                }
            }
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
