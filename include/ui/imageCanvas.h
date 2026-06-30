#ifndef SHOECOMP_IMAGE_CANVAS_H
#define SHOECOMP_IMAGE_CANVAS_H

#include "imgui.h"
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
            EBTSCanvas
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
