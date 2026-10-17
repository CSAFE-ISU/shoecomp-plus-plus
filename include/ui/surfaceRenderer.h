#ifndef SHOECOMP_UI_SURFACE_RENDERER_H
#define SHOECOMP_UI_SURFACE_RENDERER_H

#include <cstdint>
#include <memory>
#include <vector>

#include <Eigen/Core>
#include <Eigen/Geometry>
#include "formats/x3p.h"

// Offscreen OpenGL renderer for an x3p Surface3D: builds a lit,
// height-colormapped triangle mesh and draws it into a framebuffer
// whose color texture is composited by ImGui. The
// mesh/camera/coordinate math is GL-free (and unit-testable); only the
// actual GPU calls live behind the HELLOIMGUI_HAS_OPENGL guard in the
// .cpp. On builds without an OpenGL backend, supported() is false and
// render()/pick() are no-ops.
namespace shoecomp
{
    // Orbit camera around a target point. Angles in radians; distance
    // in normalized model units (the surface is scaled to ~unit size).
    struct OrbitCamera
    {
        float yaw = 0.0f;
        float pitch = 0.0f;  // 0 = top-down height-map (home) view
        float roll = 0.0f;   // tilt about the view/forward axis
        float distance = 2.6f;
        Eigen::Vector3f target = Eigen::Vector3f::Zero();
        float fovY = 0.8f;
        float nearZ = 0.02f;
        float farZ = 40.0f;

        Eigen::Vector3f eye() const;
        Eigen::Matrix4f view() const;
        Eigen::Matrix4f proj(float aspect) const;
    };

    // Maps normalized model-space coordinates (the mesh vertex space)
    // back to surface grid indices. Populated by buildSurfaceMesh.
    struct SurfaceModelParams
    {
        int sizeX = 0, sizeY = 0;
        double sx = 1.0, sy = 1.0;  // model units per column / row
        double cx = 0.0, cy = 0.0;  // grid center (columns, rows)
        float zScale = 1.0f;        // model units per Z unit
        float zc = 0.0f;  // Z center subtracted before scaling
        float zMin = 0.0f, zRange = 1.0f;

        // Nearest grid index of a model-space point. Returns false if
        // it falls outside the grid.
        bool modelToGrid(const Eigen::Vector3f& m, int& col,
                         int& row) const;
    };

    struct SurfaceMesh
    {
        // Interleaved per vertex: position(3), normal(3), colormap
        // t(1).
        std::vector<float> vertices;
        std::vector<uint32_t> indices;  // triangle list
        SurfaceModelParams params;
    };

    // Build the triangle mesh from |surf| (GL-free). Cells touching a
    // NaN (invalid) sample are dropped, leaving holes. Normals are
    // central-difference based; Z is exaggerated so shallow relief is
    // visible.
    void buildSurfaceMesh(const Surface3D& surf, SurfaceMesh& out);

    // Model-space normalization params for |surf| (GL-free). Extracted
    // from buildSurfaceMesh so callers (e.g. the canonical camera) can
    // get sx/sy/zScale without triggering the async mesh build.
    SurfaceModelParams computeModelParams(const Surface3D& surf);

    // Map a physical-space direction/displacement to model space. X/Y
    // scale by the uniform 1/maxExtent factor (Y negated, matching the
    // mesh), Z by the relief exaggeration; so the result is NOT
    // proportional to the physical vector in Z.
    Eigen::Vector3f physToModelDir(const SurfaceModelParams& P,
                                   const Surface3D& surf,
                                   const Eigen::Vector3d& d);

    // The full physical (metres) -> model-space map of |surf|, as
    // used by the mesh and modelPosOfPhysical (anisotropic: Z carries
    // the relief exaggeration).
    Eigen::Affine3d physicalToModel(const Surface3D& surf);

    class SurfaceRenderer
    {
       public:
        SurfaceRenderer();
        ~SurfaceRenderer();
        SurfaceRenderer(const SurfaceRenderer&) = delete;
        SurfaceRenderer& operator=(const SurfaceRenderer&) = delete;

        // True only on OpenGL builds (feature is desktop-GL for now).
        static bool supported();

        // Upload |surf| as the GPU mesh (GL context must be current).
        void setSurface(const Surface3D& surf);

        // Upload a pre-built mesh as the GPU mesh (GL context must be
        // current). Lets the caller build the mesh off-thread and do
        // only the upload here.
        void uploadMesh(const SurfaceMesh& mesh);

        // Render at pixel size (w,h) with |cam|; |modelXform| is an
        // extra transform applied to the surface (identity in the
        // gallery, the alignment transform in comparison). Returns the
        // color-texture handle as an ImTextureID-compatible value, or 0
        // on failure.
        uintptr_t render(int w, int h, const OrbitCamera& cam,
                         const Eigen::Affine3f& modelXform);

        // Unproject pixel (px,py; origin top-left of the last render)
        // via the depth buffer to a model-space point, then to a grid
        // index. Returns false if the pixel hit the background (no
        // surface).
        bool pick(int px, int py, const OrbitCamera& cam,
                  const Eigen::Affine3f& modelXform, int& col,
                  int& row);

        const SurfaceModelParams& params() const { return params_; }

        // Read the last-rendered frame's color buffer as top-down RGBA8
        // (row 0 = top, matching the displayed view). Returns false if
        // nothing has been rendered or on a non-GL build.
        bool readPixelsRGBA(std::vector<unsigned char>& out, int& w,
                            int& h);

        // Release all GPU resources (GL context must be current).
        void releaseGL();

       private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
        SurfaceModelParams params_;
        int lastW_ = 0, lastH_ = 0;
    };
}  // namespace shoecomp

#endif
