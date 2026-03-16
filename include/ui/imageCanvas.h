#ifndef SHOECOMP_IMAGE_CANVAS_H
#define SHOECOMP_IMAGE_CANVAS_H

#include "imgui.h"
#include "json.h"
#include <vector>

namespace shoecomp
{
    // Forward declarations
    struct LoadedImage;
    struct ImageViewState;
    enum class AnnotationMode;

    struct ImageCanvas
    {
        // Coordinate conversions
        static ImVec2 screenToImageCoord(
            ImVec2 sp, ImVec2 canvasPos, ImVec2 avail,
            ImVec2 pan, float zoom, float baseScale,
            float rotation, int imgW, int imgH);

        static ImVec2 imageToScreenCoord(
            float ix, float iy, float cx, float cy,
            float scale, float cosR, float sinR,
            int imgW, int imgH);

        // Geometry helpers
        static bool pointInPolygon(
            float px, float py,
            std::vector<jt::Json>& poly);

        static void removePointsOutsideBounds(
            LoadedImage& img);

        // Main canvas render (input + drawing)
        static void renderCanvas(
            LoadedImage& img, ImageViewState& vs,
            const char* canvasId,
            ImageViewState* linked = nullptr,
            AnnotationMode* pMode = nullptr);

        // Toolbar (zoom/pan/rotate controls +
        // annotation buttons)
        static void renderToolbar(
            LoadedImage& img, ImageViewState& vs,
            const char* toolbarId,
            AnnotationMode* mode,
            ImageViewState* linked = nullptr);
    };
}

#endif
