#include "ui/imageCanvasKinds.h"

namespace shoecomp
{
    std::unique_ptr<ImageCanvas> makeCanvas(ImageCanvas::Kind kind)
    {
        switch (kind)
        {
            case ImageCanvas::Kind::ShoeCanvas:
                return std::make_unique<ShoeCanvas>();
            case ImageCanvas::Kind::EBTSCanvas:
                return std::make_unique<EBTSCanvas>();
            case ImageCanvas::Kind::HWCanvas:
                return std::make_unique<HWCanvas>();
            case ImageCanvas::Kind::X3PBulletLand:
                return std::make_unique<X3PBulletLand>();
            case ImageCanvas::Kind::X3P:
            case ImageCanvas::Kind::Canvas3D:
                return std::make_unique<X3PImage>();
            case ImageCanvas::Kind::Canvas2D:
            default:
                return std::make_unique<ImageCanvas2D>();
        }
    }
}  // namespace shoecomp
