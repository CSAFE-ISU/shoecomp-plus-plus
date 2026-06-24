#ifndef SHOECOMP_IMAGE_CANVAS_2D_H
#define SHOECOMP_IMAGE_CANVAS_2D_H

#include "ui/imageCanvas.h"
#include "jtjson/json.h"
#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace shoecomp
{
    // Concrete 2D image canvas: owns a shared ImageData plus a
    // per-canvas ImageViewState, and implements the ImageCanvas
    // interface. Gallery images each own one; comparison viewers
    // share the same ImageData via shared_ptr but keep independent
    // view state.
    struct ImageCanvas2D : public ImageCanvas
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

        // --- Data members ---
        std::shared_ptr<ImageData> image;
        ImageViewState viewState;

        // --- Shared annotation style ---
        static AnnotationStyle style;

        ImageCanvas2D();
        explicit ImageCanvas2D(std::shared_ptr<ImageData> img);

        // --- ImageCanvas interface ---
        Kind kind() const override { return Kind::Canvas2D; }

        bool hasImage() const override { return image != nullptr; }
        int width() const override { return image ? image->width : 0; }
        int height() const override
        {
            return image ? image->height : 0;
        }
        const std::string& name() const override;
        const std::string& path() const override;
        const std::vector<std::string>& imageExtensions()
            const override;
        void resetView() override;
        void renderCanvas(const char* canvasId) override;
        void renderToolbar(const char* toolbarId) override;

        ViewTargets snapshotTargets() const override;
        void renderViewerPanel(
            std::vector<std::unique_ptr<ImageCanvas>>& images,
            int& selectedIdx, int otherIdx, bool locked,
            const char* label) override;
        void applyAlignment(ImageCanvas& other, const AlignState& a,
                            bool& locked) override;
        void syncPair(ImageCanvas& other, const ViewTargets& self0,
                      const ViewTargets& other0, const AlignState& a,
                      bool& locked, bool alignDialogOpen,
                      const SettingsState& settings) override;
        void clearHomeRequested() override;

        // --- PointType <-> string ---
        static const char* pointTypeToString(PointType t);
        static PointType stringToPointType(const std::string& s);

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

        // Instance helpers
        ImVec2 getImageCoord(ImVec2 sp) const;

        void removePointsOutsideBounds();

        void renderAnnotations(ImDrawList* dl, float cx, float cy,
                               float annScale, float rotation,
                               bool hovered, AnnotationMode mode,
                               const ImGuiIO& io);

        // --- Settings UI owned by the canvas ---
        // Renders the annotation-style ("Annotations") settings
        // section (edits the shared `style`).
        static void renderStyleSettings();

       private:
        // Sync helpers, used only between same-Kind 2D viewers.
        void syncLockedViewers(ImageCanvas2D& other,
                               const AlignState& a,
                               const ViewTargets& self0,
                               const ViewTargets& other0);
        void renderLockedCursorIndicators(
            ImageCanvas2D& other, const AlignState& align,
            const SettingsState& settings);
    };

    // Narrowing helper for the few call sites that need 2D
    // internals through a base reference/pointer. Returns nullptr
    // if the canvas is not a 2D canvas.
    inline ImageCanvas2D* asCanvas2D(ImageCanvas& c)
    {
        return c.kind() == ImageCanvas::Kind::Canvas2D
                   ? static_cast<ImageCanvas2D*>(&c)
                   : nullptr;
    }

    inline const ImageCanvas2D* asCanvas2D(const ImageCanvas& c)
    {
        return c.kind() == ImageCanvas::Kind::Canvas2D
                   ? static_cast<const ImageCanvas2D*>(&c)
                   : nullptr;
    }
}  // namespace shoecomp

#endif
