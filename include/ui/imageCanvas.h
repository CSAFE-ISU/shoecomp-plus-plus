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
    // Calculation/app types used by ImageCanvas's static
    // helpers. Passed by reference, so forward declarations
    // suffice in this header; the .cpp includes the full
    // definitions.
    struct AlignState;
    struct SettingsState;

    struct ImageCanvas
    {
        // --- Nested canvas-owned types ---
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

            // Resets annotations to an empty object with empty
            // bounds/points arrays.
            void resetAnnotations();
        };

        // Snapshot of a viewer's animation targets, captured
        // before rendering so syncPair can detect which viewer
        // the user changed.
        struct ViewTargets
        {
            float zoom = 0.0f;
            ImVec2 pan = ImVec2(0, 0);
            float rotation = 0.0f;
        };

        // --- Data members ---
        std::shared_ptr<ImageData> image;
        ImageViewState viewState;
        bool minimized = false;
        bool lastMinimized = false;

        // --- Shared annotation style ---
        static AnnotationStyle style;

        ImageCanvas();
        explicit ImageCanvas(std::shared_ptr<ImageData> img);

        // --- Supported image file extensions ---
        const std::vector<std::string>& imageExtensions() const;

        // --- PointType <-> string ---
        static const char* pointTypeToString(PointType t);
        static PointType stringToPointType(const std::string& s);

        // --- Lightweight accessors for the generic GUI ---
        bool hasImage() const { return image != nullptr; }
        int width() const { return image ? image->width : 0; }
        int height() const { return image ? image->height : 0; }
        const std::string& name() const;

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

        // Resets pan/zoom/rotation (and their targets) to the
        // identity home view.
        void resetView();

        void renderAnnotations(ImDrawList* dl, float cx, float cy,
                               float annScale, float rotation,
                               bool hovered, AnnotationMode mode,
                               const ImGuiIO& io);

        void renderCanvas(const char* canvasId);

        void renderToolbar(const char* toolbarId);

        // --- Comparison-viewer GUI (Image Comparison tab) ---

        // Renders the combo + canvas + toolbar for one side of
        // the comparison view. Pokes only this canvas's own
        // internals.
        static void renderViewerPanel(std::vector<ImageCanvas>& images,
                                      int& selectedIdx, int otherIdx,
                                      ImageCanvas& viewer, bool locked,
                                      const char* label);

        // Captures a viewer's animation targets before render.
        static ViewTargets snapshotTargets(const ImageCanvas& c);

        // --- Pair / alignment logic ---

        static void applyAlignment(ImageCanvas& left,
                                   ImageCanvas& right,
                                   const AlignState& a, bool& locked);

        static void syncLockedViewers(ImageCanvas& left,
                                      ImageCanvas& right,
                                      const AlignState& a,
                                      const ViewTargets& l0,
                                      const ViewTargets& r0);

        static void renderLockedCursorIndicators(
            const ImageCanvas& left, const ImageCanvas& right,
            const AlignState& align, const SettingsState& settings);

        // Post-render locked sync: handles the Home one-shot,
        // otherwise propagates the changed viewer through the
        // alignment, then draws the cursor indicators. Clears
        // both homeRequested flags.
        static void syncPair(ImageCanvas& left, ImageCanvas& right,
                             const ViewTargets& l0,
                             const ViewTargets& r0,
                             const AlignState& align, bool& locked,
                             bool alignDialogOpen,
                             const SettingsState& settings);

        // --- Settings UI owned by the canvas ---

        // Renders the annotation-style ("Annotations") settings
        // section (edits the shared `style`).
        static void renderStyleSettings();
    };
}  // namespace shoecomp

#endif
