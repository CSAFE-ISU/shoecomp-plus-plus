#ifndef SHOECOMP_EBTS_CANVAS_H
#define SHOECOMP_EBTS_CANVAS_H

#include "ui/imageCanvas2d.h"
#include <memory>
#include <string>
#include <vector>

namespace shoecomp
{
    // 2D canvas for NIST/EBTS workflows: loads PNG and the
    // ebts/nist container formats (one file may yield many images).
    struct EBTSCanvas : public ImageCanvas2D
    {
        Kind kind() const override { return Kind::EBTSCanvas; }
        std::vector<PointType> allowedPointTypes() const override;
        const std::vector<std::string>& imageExtensions()
            const override;
        int loadImages(const std::string& path,
                       std::vector<std::unique_ptr<ImageCanvas>>& out,
                       std::string& err) const override;
    };
}  // namespace shoecomp

#endif
