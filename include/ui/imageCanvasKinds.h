#ifndef SHOECOMP_IMAGE_CANVAS_KINDS_H
#define SHOECOMP_IMAGE_CANVAS_KINDS_H

#include "ui/imageCanvas2d.h"
#include <memory>
#include <string>
#include <vector>

namespace shoecomp
{
    // 2D canvas for shoeprint workflows: loads PNG and JPEG.
    struct ShoeCanvas : public ImageCanvas2D
    {
        Kind kind() const override { return Kind::ShoeCanvas; }
        const std::vector<std::string>& imageExtensions()
            const override;
        int loadImages(const std::string& path,
                       std::vector<std::unique_ptr<ImageCanvas>>& out,
                       std::string& err) const override;
    };

    // 2D canvas for NIST/EBTS workflows: loads PNG and the
    // ebts/nist container formats (one file may yield many images).
    struct EBTSCanvas : public ImageCanvas2D
    {
        Kind kind() const override { return Kind::EBTSCanvas; }
        const std::vector<std::string>& imageExtensions()
            const override;
        int loadImages(const std::string& path,
                       std::vector<std::unique_ptr<ImageCanvas>>& out,
                       std::string& err) const override;
    };

    // Construct a fresh 2D canvas of the given kind. Unknown/Canvas2D
    // falls back to a plain ImageCanvas2D.
    std::unique_ptr<ImageCanvas2D> makeCanvas(ImageCanvas::Kind kind);
}  // namespace shoecomp

#endif
