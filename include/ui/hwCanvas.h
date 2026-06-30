#ifndef SHOECOMP_HW_CANVAS_H
#define SHOECOMP_HW_CANVAS_H

#include "ui/imageCanvas2d.h"
#include <memory>
#include <string>
#include <vector>

namespace shoecomp
{
    // 2D canvas for handwriting annotation workflows: loads PNG and
    // JPEG. Allows WordStart, WordEnd, Intersection, and CurveTurn
    // point types.
    struct HWCanvas : public ImageCanvas2D
    {
        Kind kind() const override { return Kind::HWCanvas; }
        std::vector<PointType> allowedPointTypes() const override;
        const std::vector<std::string>& imageExtensions()
            const override;
        int loadImages(const std::string& path,
                       std::vector<std::unique_ptr<ImageCanvas>>& out,
                       std::string& err) const override;
    };
}  // namespace shoecomp

#endif
