#ifndef SHOECOMP_IMAGE_CANVAS_KINDS_H
#define SHOECOMP_IMAGE_CANVAS_KINDS_H

#include "ui/shoeCanvas.h"
#include "ui/ebtsCanvas.h"
#include "ui/hwCanvas.h"
#include "ui/imageCanvas3d.h"
#include <memory>

namespace shoecomp
{
    // Construct a fresh canvas of the given kind (returned by base
    // pointer so both 2D and 3D kinds flow through the same active-kind
    // pipeline). Unknown/Canvas2D falls back to a plain ImageCanvas2D.
    std::unique_ptr<ImageCanvas> makeCanvas(ImageCanvas::Kind kind);
}  // namespace shoecomp

#endif
