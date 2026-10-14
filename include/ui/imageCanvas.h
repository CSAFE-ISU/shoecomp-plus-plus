#ifndef SHOECOMP_IMAGE_CANVAS_H
#define SHOECOMP_IMAGE_CANVAS_H

#include "imgui.h"
#include "core/imageType.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace shoecomp
{
    // Calculation/app types used by ImageCanvas's comparison
    // interface. Passed by reference, so forward declarations
    // suffice in this header; subclass .cpp files include the
    // full definitions.
    struct AlignState;
    struct SettingsState;

    // Abstract base for all image canvases. Concrete subclasses
    // (e.g. ImageCanvas2D) own the actual image data + view state
    // and implement this interface. `Kind` tags the concrete type
    // so callers can distinguish subclasses; only same-Kind
    // instances are ever compared together.
    struct ImageCanvas
    {
        enum class Kind
        {
            Canvas2D,
            ShoeCanvas,
            EBTSCanvas,
            HWCanvas,
            Canvas3D,
            X3P,
            X3PBulletLand
        };

        using PointType = shoecomp::PointType;
        using DetectionSpec = shoecomp::DetectionSpec;

        // Snapshot of a viewer's animation targets, captured
        // before rendering so syncPair can detect which viewer
        // the user changed.
        struct ViewTargets
        {
            float zoom = 0.0f;
            ImVec2 pan = ImVec2(0, 0);
            float rotation = 0.0f;
        };

        // Window-collapse state, common to every canvas kind.
        // ImGui's own collapsed flag is the source of truth; these
        // mirror it for the gallery + image-list dialog.
        bool minimized = false;
        bool lastMinimized = false;

        // Per-window Detect button state (one-shot, like the view's
        // homeRequested). The tab loop sets detectAvailable each frame
        // (whether the source canvas can be detected on); the toolbar
        // draws the button gated on it and sets detectRequested on
        // click; the tab loop consumes detectRequested and opens the
        // dialog.
        bool detectAvailable = false;
        bool detectRequested = false;

        virtual ~ImageCanvas() = default;

        // Returns the annotation point types this canvas kind accepts.
        // Used to filter settings color pickers and validate JSON
        // loads.
        virtual std::vector<PointType> allowedPointTypes() const = 0;

        // Renders the annotation-style settings section for this canvas
        // kind (edits the shared AnnotationStyle). Only controls for
        // point types returned by allowedPointTypes() are shown.
        virtual void renderAnnotationStyleSettings() = 0;

        // --- Subclass tag ---
        virtual Kind kind() const = 0;

        // True for any canvas in the ImageCanvas2D family. Used by
        // asCanvas2D() to narrow safely regardless of the concrete
        // 2D Kind (Canvas2D / ShoeCanvas / EBTSCanvas).
        virtual bool is2D() const { return false; }

        // True for any canvas in the ImageCanvas3D family (x3p
        // surfaces). Used by asCanvas3D() to narrow safely.
        virtual bool is3D() const { return false; }

        // --- Optional ONNX detection interface ---
        // Whether this canvas kind can run automatic point detection.
        // Kinds that support it also override detectionSpec(). The
        // base leaves the feature off so existing kinds are unaffected.
        virtual bool supportsDetection() const { return false; }

        // The detection configuration for this canvas kind (input size,
        // thresholds, class -> PointType map). Only meaningful when
        // supportsDetection() is true.
        virtual DetectionSpec detectionSpec() const { return {}; }

        // --- Clean API used by the generic GUI ---
        virtual bool hasImage() const = 0;
        virtual int width() const = 0;
        virtual int height() const = 0;
        virtual const std::string& name() const = 0;
        virtual const std::string& path() const = 0;
        virtual const std::vector<std::string>& imageExtensions()
            const = 0;
        virtual void resetView() = 0;
        virtual void renderCanvas(const char* canvasId) = 0;
        virtual void renderToolbar(const char* toolbarId) = 0;

        // --- Load / annotation persistence (generic across kinds) ---
        // Prototype/factory: parse |path|, append the constructed
        // canvas(es) to |out|, and return the count (or -1 with |err|
        // set). Save/load the current image's annotations to/from a
        // JSON file, returning 0 on success. The base leaves these
        // unsupported; ImageCanvas2D and ImageCanvas3D override them.
        // Capture the current rendered view as top-down RGBA8 into
        // |out| (sizing it), returning the dimensions in |w|/|h|. Used
        // by Save PNG. The base returns false (unsupported).
        virtual bool captureViewRGBA(std::vector<unsigned char>& out,
                                     int& w, int& h) const
        {
            (void)out;
            (void)w;
            (void)h;
            return false;
        }

        virtual int loadImages(
            const std::string& path,
            std::vector<std::unique_ptr<ImageCanvas>>& out,
            std::string& err) const
        {
            (void)path;
            (void)out;
            err = "loading is not supported for this canvas kind";
            return -1;
        }
        virtual int saveAnnotations(const std::string& path) const
        {
            (void)path;
            return -1;
        }
        virtual int loadAnnotations(const std::string& path,
                                    std::string& err)
        {
            (void)path;
            err = "annotations are not supported for this canvas kind";
            return -1;
        }

        // --- Comparison-viewer interface ---
        // These operate on `this` viewer plus a same-Kind partner.
        // They are virtual so each subclass can specialize the
        // pairing/alignment behaviour.

        // Captures this viewer's animation targets before render.
        virtual ViewTargets snapshotTargets() const = 0;

        // Renders the combo + canvas + toolbar for this viewer
        // (one side of the comparison view).
        virtual void renderViewerPanel(
            std::vector<std::unique_ptr<ImageCanvas>>& images,
            int& selectedIdx, int otherIdx, bool locked,
            const char* label) = 0;

        // Sets `other` to match this viewer + the alignment offset.
        virtual void applyAlignment(ImageCanvas& other,
                                    const AlignState& a,
                                    bool& locked) = 0;

        // Post-render locked sync: handles the Home one-shot,
        // otherwise propagates the changed viewer through the
        // alignment, then draws the cursor indicators. `self0`/
        // `other0` are the pre-render snapshots of this/other.
        virtual void syncPair(ImageCanvas& other,
                              const ViewTargets& self0,
                              const ViewTargets& other0,
                              const AlignState& a, bool& locked,
                              bool alignDialogOpen,
                              const SettingsState& settings) = 0;

        // Clears this viewer's pending Home one-shot.
        virtual void clearHomeRequested() = 0;
    };
}  // namespace shoecomp

#endif
