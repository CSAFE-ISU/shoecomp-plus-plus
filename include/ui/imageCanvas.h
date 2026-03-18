#ifndef SHOECOMP_IMAGE_CANVAS_H
#define SHOECOMP_IMAGE_CANVAS_H

#include "imgui.h"
#include "jtjson/json.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace shoecomp
{
    enum class AnnotationMode
    {
        None,
        AddPoint,
        AddBounds
    };

    enum class PointType : uint8_t
    {
        Corner = 0,
        Center = 1
    };

    const char* pointTypeToString(PointType t);
    PointType stringToPointType(
        const std::string& s);

    struct AnnotationStyle
    {
        float pointRadius = 5.0f;
        float cornerColor[4] = {
            1.0f, 0.2f, 0.2f, 0.86f};
        float centerColor[4] = {
            0.2f, 0.39f, 1.0f, 0.86f};
        float boundsLineThickness = 2.0f;
        float boundsColor[4] = {
            0.2f, 1.0f, 0.2f, 0.86f};
    };

    extern AnnotationStyle g_annotationStyle;

    struct ImageViewState
    {
        float zoom = 1.0f;
        float zoomTarget = 1.0f;
        ImVec2 pan = ImVec2(0, 0);
        ImVec2 panTarget = ImVec2(0, 0);
        float rotation = 0.0f;
        float rotationTarget = 0.0f;
    };

    struct ImageData
    {
        std::string name;
        std::string path;
        ImTextureID textureId = 0;
        int width = 0;
        int height = 0;
        jt::Json annotations;
        AnnotationMode annotationMode =
            AnnotationMode::None;
        PointType selectedPointType =
            PointType::Corner;

        ~ImageData();
    };

    struct ImageCanvas
    {
        std::shared_ptr<ImageData> image;
        ImageViewState viewState;

        ImageCanvas();
        explicit ImageCanvas(
            std::shared_ptr<ImageData> img);

        // Static math helpers (pure functions)
        static ImVec2 screenToImageCoord(
            ImVec2 sp, ImVec2 canvasPos, ImVec2 avail,
            ImVec2 pan, float zoom, float baseScale,
            float rotation, int imgW, int imgH);

        static ImVec2 imageToScreenCoord(
            float ix, float iy, float cx, float cy,
            float scale, float cosR, float sinR,
            int imgW, int imgH);

        static bool pointInPolygon(
            float px, float py,
            std::vector<jt::Json>& poly);

        // Instance methods
        void removePointsOutsideBounds();

        void renderCanvas(
            const char* canvasId,
            ImageViewState* linked = nullptr);

        void renderToolbar(
            const char* toolbarId,
            ImageViewState* linked = nullptr);
    };
}

#endif
