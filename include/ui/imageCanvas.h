#ifndef SHOECOMP_IMAGE_CANVAS_H
#define SHOECOMP_IMAGE_CANVAS_H

#include "imgui.h"
#include "jtjson/json.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

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
        Center = 1,
        RidgeEnding = 2,
        Bifurcation = 3,
        Other = 4,
        Core = 5,
        Delta = 6
    };

    const char* pointTypeToString(PointType t);
    PointType stringToPointType(const std::string& s);

    struct AnnotationStyle
    {
        float pointRadius = 5.0f;
        ImVec4 cornerColor = ImVec4(1.0f, 0.2f, 0.2f, 0.86f);
        ImVec4 centerColor = ImVec4(0.2f, 0.39f, 1.0f, 0.86f);
        ImVec4 ridgeEndingColor = ImVec4(1.0f, 0.85f, 0.0f, 0.86f);
        ImVec4 bifurcationColor = ImVec4(1.0f, 0.55f, 0.0f, 0.86f);
        ImVec4 otherColor = ImVec4(0.8f, 0.8f, 0.0f, 0.86f);
        ImVec4 coreColor = ImVec4(0.0f, 0.85f, 0.85f, 0.86f);
        ImVec4 deltaColor = ImVec4(0.85f, 0.0f, 0.85f, 0.86f);
        float boundsLineThickness = 2.0f;
        ImVec4 boundsColor = ImVec4(0.2f, 1.0f, 0.2f, 0.86f);
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

        // Locked indicator state
        bool isHovered = false;
        ImVec2 canvasPos = ImVec2(0, 0);
        ImVec2 canvasSize = ImVec2(0, 0);
        float baseScale = 1.0f;
        float renderScale = 1.0f;

        // One-shot flag set by the toolbar Home
        // button; consumed by the locked-viewer sync.
        bool homeRequested = false;

        bool contains(const ImVec2& pt) const;
        ImVec2 canvasCenter() const;
    };

    struct ImageData
    {
        std::string name;
        std::string path;
        ImTextureID textureId = 0;
        int width = 0;
        int height = 0;
        jt::Json annotations;
        AnnotationMode annotationMode = AnnotationMode::None;
        PointType selectedPointType = PointType::Corner;

        ~ImageData();
    };

    struct ImageCanvas
    {
        std::shared_ptr<ImageData> image;
        ImageViewState viewState;
        bool minimized = false;
        bool lastMinimized = false;

        ImageCanvas();
        explicit ImageCanvas(std::shared_ptr<ImageData> img);

        // Static math helpers (pure functions)
        static ImVec2 screenToImageCoord(ImVec2 sp, ImVec2 canvasPos,
                                         ImVec2 avail, ImVec2 pan,
                                         float zoom, float baseScale,
                                         float rotation, int imgW,
                                         int imgH);

        static ImVec2 imageToScreenCoord(float ix, float iy, float cx,
                                         float cy, float scale,
                                         float rotation, int imgW,
                                         int imgH);

        static bool pointInPolygon(float px, float py,
                                   const std::vector<jt::Json>& poly);

        // Instance methods
        ImVec2 getImageCoord(ImVec2 sp) const;

        void removePointsOutsideBounds();

        void renderAnnotations(ImDrawList* dl, float cx, float cy,
                               float annScale, float rotation,
                               bool hovered, AnnotationMode mode,
                               const ImGuiIO& io);

        void renderCanvas(const char* canvasId);

        void renderToolbar(const char* toolbarId);
    };
}  // namespace shoecomp

#endif
