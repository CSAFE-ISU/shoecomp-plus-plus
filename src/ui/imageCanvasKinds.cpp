#include "ui/imageCanvasKinds.h"

namespace shoecomp
{
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
