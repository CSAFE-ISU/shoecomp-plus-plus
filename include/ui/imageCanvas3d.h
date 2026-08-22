#ifndef SHOECOMP_UI_IMAGE_CANVAS_3D_H
#define SHOECOMP_UI_IMAGE_CANVAS_3D_H

#include <memory>
#include <string>
#include <vector>

#include "aligncalc/eigen/Eigen/Geometry"
#include "formats/x3p.h"
#include "ui/imageCanvas.h"
#include "ui/surfaceRenderer.h"

namespace shoecomp
{
    // Manual 3D alignment for the comparison workspace. See
    // .claude/x3p-notes.md.
    struct Align3DState
    {
        Eigen::Vector3f translation = Eigen::Vector3f::Zero();
        Eigen::Vector3f rotationDeg = Eigen::Vector3f::Zero();
        float scale = 1.0f;
        jt::Json info;

        Eigen::Affine3f matrix() const;
    };

    // Base for 3D surface canvases. Owns the Surface3D (shared, like
    // ImageCanvas2D::ImageData), an orbit camera, and an OpenGL
    // SurfaceRenderer. Implements the ImageCanvas interface with an
    // interactive 3D view. X3PImage is the concrete x3p kind.
    struct ImageCanvas3D : public ImageCanvas
    {
        enum class Mode
        {
            None,
            AddPoint,
            AddBounds
        };

        std::shared_ptr<Surface3D> surface;
        OrbitCamera camera;
        SurfaceRenderer renderer;

        // Whether the user moved the camera this frame (for locked
        // camera sync in the comparison workspace).
        bool cameraDirty = false;
        // The surface the renderer's GPU mesh was last built from, so
        // shared viewers rebuild only when their selection changes.
        const Surface3D* meshSource = nullptr;

        // Model transform for the comparison-viewer path (identity in
        // the gallery).
        Eigen::Affine3f compareXform = Eigen::Affine3f::Identity();

        // Shared markup mode (mirrors ImageCanvas2D's static markup
        // state), reset on active-kind change.
        static Mode markupMode;

        ImageCanvas3D();
        explicit ImageCanvas3D(std::shared_ptr<Surface3D> surf);
        ~ImageCanvas3D() override;

        // --- tags ---
        Kind kind() const override { return Kind::Canvas3D; }
        bool is3D() const override { return true; }

        // --- annotation types / style ---
        std::vector<PointType> allowedPointTypes() const override
        {
            return {PointType::Peak};
        }
        void renderAnnotationStyleSettings() override;

        // --- clean API ---
        bool hasImage() const override { return surface != nullptr; }
        int width() const override
        {
            return surface ? surface->sizeX : 0;
        }
        int height() const override
        {
            return surface ? surface->sizeY : 0;
        }
        const std::string& name() const override;
        const std::string& path() const override;
        const std::vector<std::string>& imageExtensions()
            const override;
        void resetView() override;
        void renderCanvas(const char* canvasId) override;
        void renderToolbar(const char* toolbarId) override;

        // --- annotation persistence (both {x,y,z} + {col,row}) ---
        int saveAnnotations(const std::string& path) const override;
        int loadAnnotations(const std::string& path,
                            std::string& err) override;

        bool captureViewRGBA(std::vector<unsigned char>& out, int& w,
                             int& h) const override;

        // --- comparison API ---
        ViewTargets snapshotTargets() const override
        {
            return ViewTargets{};
        }
        void renderViewerPanel(
            std::vector<std::unique_ptr<ImageCanvas>>& images,
            int& selectedIdx, int otherIdx, bool locked,
            const char* label) override;
        // 2D-typed alignment API is unused for 3D (the comparison path
        // routes through the 3D-specific methods below); these satisfy
        // the interface as no-ops.
        void applyAlignment(ImageCanvas&, const AlignState&,
                            bool&) override
        {
        }
        void syncPair(ImageCanvas&, const ViewTargets&,
                      const ViewTargets&, const AlignState&, bool&,
                      bool, const SettingsState&) override
        {
        }
        void clearHomeRequested() override {}

        // --- 3D helpers ---
        // Draw the 3D view + annotation overlay into the current ImGui
        // region. |modelXform| is identity in the gallery; the
        // alignment transform in comparison. |allowMarkup| enables
        // click-to-place.
        void render3DView(const char* id,
                          const Eigen::Affine3f& modelXform,
                          bool allowMarkup);
        // Model-space position of a grid sample (for overlay
        // projection).
        Eigen::Vector3f modelPosOf(int col, int row) const;
        void ensureMesh();  // (re)build GPU mesh if the surface changed

        // Copy the orbit camera to/from |other| toward whichever side
        // moved it this frame (when locked).
        void sync3DCameras(ImageCanvas3D& other, bool locked);

       private:
        void addAnnotation(int col, int row);
        void removeNearestPoint(int col, int row);
    };

    // Concrete x3p surface canvas.
    struct X3PImage : public ImageCanvas3D
    {
        using ImageCanvas3D::ImageCanvas3D;
        Kind kind() const override { return Kind::X3P; }
        const std::vector<std::string>& imageExtensions()
            const override;
        int loadImages(const std::string& path,
                       std::vector<std::unique_ptr<ImageCanvas>>& out,
                       std::string& err) const override;
    };

    // Narrowing helpers, mirroring asCanvas2D().
    inline ImageCanvas3D* asCanvas3D(ImageCanvas& c)
    {
        return c.is3D() ? static_cast<ImageCanvas3D*>(&c) : nullptr;
    }
    inline const ImageCanvas3D* asCanvas3D(const ImageCanvas& c)
    {
        return c.is3D() ? static_cast<const ImageCanvas3D*>(&c)
                        : nullptr;
    }
}  // namespace shoecomp

#endif
