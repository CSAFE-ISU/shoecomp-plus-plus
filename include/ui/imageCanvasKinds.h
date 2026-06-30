#ifndef SHOECOMP_IMAGE_CANVAS_KINDS_H
#define SHOECOMP_IMAGE_CANVAS_KINDS_H

#include "ui/shoeCanvas.h"
#include "ui/ebtsCanvas.h"
#include <memory>

namespace shoecomp
{
    // Construct a fresh 2D canvas of the given kind. Unknown/Canvas2D
    // falls back to a plain ImageCanvas2D.
    std::unique_ptr<ImageCanvas2D> makeCanvas(ImageCanvas::Kind kind);
}  // namespace shoecomp

#endif
