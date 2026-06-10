#include "ui/imageCanvas.h"
#include "ui/alignDialog.h"
#include "ui/settings.h"
#include "ui/uiHelpers.h"
#include "ui/calcHelpers.h"

namespace shoecomp
{

    // Apply alignment transform: set right viewer
    // to match left viewer + alignment offset.
    void ImageCanvas::applyAlignment(ImageCanvas& viewerLeft,
                                     ImageCanvas& viewerRight,
                                     const AlignState& a, bool& locked)
    {
        auto& lv = viewerLeft.viewState;
        auto& rv = viewerRight.viewState;

        if (!viewerLeft.image || !viewerRight.image)
        {
            locked = false;
            return;
        }

        locked = false;
        rv.zoomTarget = lv.zoomTarget * a.scale;
        rv.rotationTarget = lv.rotationTarget + a.rotation;

        // Use each viewer's canvas center as reference point
        ImVec2 leftRefScreen = lv.canvasCenter();
        ImVec2 rightRefScreen = rv.canvasCenter();

        // Convert left reference to image coordinates
        ImVec2 leftImgPt = viewerLeft.getImageCoord(leftRefScreen);

        // Apply alignment transformation from left to right
        ImVec2 rightImgPt = a.transformLeft2Right(leftImgPt);

        // Start with same pan as left, then compute adjustment
        rv.panTarget = lv.panTarget;

        // Compute right viewer rendering state
        float rightRenderScale = rv.baseScale * rv.zoomTarget;
        ImVec2 rightCenter = rv.canvasCenter() + rv.panTarget;

        // Convert right image point back to screen coordinates
        ImVec2 rightScreen = ImageCanvas::imageToScreenCoord(
            rightImgPt.x, rightImgPt.y, rightCenter.x, rightCenter.y,
            rightRenderScale, rv.rotationTarget,
            viewerRight.image->width, viewerRight.image->height);

        // Adjust pan so right image point appears at right canvas
        // center
        rv.panTarget = rightRefScreen - rightScreen;

        locked = true;
    }

    static void propagateZoom(ImageCanvas& src, ImageCanvas& dst,
                              const AlignState& a, bool srcIsLeft,
                              const ImageCanvas::ViewTargets& dst0)
    {
        auto& srcView = src.viewState;
        auto& dstView = dst.viewState;
        ImVec2 srcImgPt;
        ImVec2 dstImgPt;
        float oldDstScale;
        ImVec2 oldDstCenter, oldDstScreen;
        float newDstScale;
        ImVec2 newDstCenter, newDstScreen;

        ImVec2 cursor = ImGui::GetIO().MousePos;
        srcImgPt = src.getImageCoord(cursor);

        oldDstScale = dstView.baseScale * dst0.zoom;
        oldDstCenter = dstView.canvasCenter() + dst0.pan;

        if (srcIsLeft)
        {
            dstImgPt = a.transformLeft2Right(srcImgPt);
            dstView.zoomTarget = srcView.zoomTarget * a.scale;
            dstView.rotationTarget =
                srcView.rotationTarget + a.rotation;
            dstView.panTarget = dst0.pan;
        }
        else
        {
            dstImgPt = a.transformRight2Left(srcImgPt);
            dstView.zoomTarget = srcView.zoomTarget / a.scale;
            dstView.rotationTarget =
                srcView.rotationTarget - a.rotation;
            dstView.panTarget = dst0.pan;
        }

        oldDstScreen = ImageCanvas::imageToScreenCoord(
            dstImgPt.x, dstImgPt.y, oldDstCenter.x, oldDstCenter.y,
            oldDstScale, dst0.rotation, dst.image->width,
            dst.image->height);
        //
        newDstScale = dstView.baseScale * dstView.zoomTarget;
        newDstCenter = dstView.canvasCenter() + dst0.pan;
        newDstScreen = ImageCanvas::imageToScreenCoord(
            dstImgPt.x, dstImgPt.y, newDstCenter.x, newDstCenter.y,
            newDstScale, dstView.rotationTarget, dst.image->width,
            dst.image->height);

        dstView.panTarget += (oldDstScreen - newDstScreen);
    }

    // Sync locked viewers after rendering. Detects
    // which viewer changed and propagates through
    // alignment.
    void ImageCanvas::syncLockedViewers(ImageCanvas& viewerLeft,
                                        ImageCanvas& viewerRight,
                                        const AlignState& a,
                                        const ViewTargets& l0,
                                        const ViewTargets& r0)
    {
        auto& lv = viewerLeft.viewState;
        auto& rv = viewerRight.viewState;

        if (!viewerLeft.image || !viewerRight.image) return;

        float aRad = a.rotation;
        bool lChanged = lv.zoomTarget != l0.zoom ||
                        lv.panTarget.x != l0.pan.x ||
                        lv.panTarget.y != l0.pan.y ||
                        lv.rotationTarget != l0.rotation;
        bool rChanged = rv.zoomTarget != r0.zoom ||
                        rv.panTarget.x != r0.pan.x ||
                        rv.panTarget.y != r0.pan.y ||
                        rv.rotationTarget != r0.rotation;

        bool lZoomed = lv.zoomTarget != l0.zoom;
        bool rZoomed = rv.zoomTarget != r0.zoom;

        if ((lChanged && !rChanged) || (lChanged && rChanged))
        {
            if (lZoomed)
            {
                propagateZoom(/*src=*/viewerLeft,
                              /*dst=*/viewerRight,
                              /*align*/ a, /*srcIsLeft*/ true,  //
                              r0);
            }
            else
            {
                rv.zoomTarget = lv.zoomTarget * a.scale;
                rv.zoom = lv.zoom * a.scale;
                rv.panTarget += (lv.panTarget - l0.pan);
                rv.rotationTarget = lv.rotationTarget + aRad;
                rv.rotation = lv.rotation + aRad;
            }
        }
        else if (rChanged && !lChanged)
        {
            if (rZoomed)
            {
                propagateZoom(/*src=*/viewerRight,
                              /*dst=*/viewerLeft,
                              /*align*/ a, /*srcIsLeft*/ false,  //
                              l0);
            }
            else
            {
                lv.zoomTarget = rv.zoomTarget / a.scale;
                lv.zoom = rv.zoom / a.scale;
                lv.panTarget += (rv.panTarget - r0.pan);
                lv.rotationTarget = rv.rotationTarget - aRad;
                lv.rotation = rv.rotation - aRad;
            }
        }
    }

    static void renderLockedCursorIndicators0(
        const AlignState& align, const ImageCanvas& src,
        const std::vector<jt::Json>& srcPoints, const ImageCanvas& dst,
        const std::vector<jt::Json>& dstPoints, bool srcIsLeft,
        const SettingsState& settings)
    {
        // Visual constants (from settings)
        const float primaryRadius = settings.cursorRadius;
        const ImU32 primaryFill =
            ImGui::ColorConvertFloat4ToU32(settings.cursorColor);
        const ImU32 primaryOutline = kColorCursorOutline;
        const float secondaryRadius = settings.correspondingRadius;
        const ImU32 correspondingColor =
            ImGui::ColorConvertFloat4ToU32(settings.correspondingColor);
        const float hoverThreshold = settings.hoverThreshold;

        // Transformed cursor indicator constants
        const float transformedRadius = settings.cursorRadius;
        const ImU32 transformedFill =
            ImGui::ColorConvertFloat4ToU32(settings.transformedColor);
        const ImU32 transformedOutline = kColorCursorOutline;  // White
        const ImU32 transformedTextColor =
            ImGui::ColorConvertFloat4ToU32(
                ImVec4(settings.transformedColor.x,
                       settings.transformedColor.y,
                       settings.transformedColor.z, 1.0f));

        ImGuiIO& io = ImGui::GetIO();
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        ImVec2 transformed;
        char coordText[64];
        char transformedText[64];
        char matchedText[64];
        float srcX, srcY;
        ImVec2 srcScreenPos;
        float dstX, dstY;
        ImVec2 dstScreenPos;
        float dist;
        //

        auto& srcView = src.viewState;
        ImVec2 srcAdjustCenter = srcView.canvasCenter() + srcView.pan;
        auto& srcImg = src.image;
        auto& dstView = dst.viewState;
        auto& dstImg = dst.image;
        ImVec2 dstAdjustCenter = dstView.canvasCenter() + dstView.pan;
        const size_t N = srcPoints.size();

        // Get cursor position in image coordinates. Invert
        // using the CURRENTLY rendered state (pan/zoom/rotation),
        // not the animation targets, so the cursor's image
        // coordinate matches what is drawn this frame (the image
        // and the matched-point circles below both use the
        // current state). Using getImageCoord() here would read
        // the targets and desync the indicators during/after any
        // zoom or pan.
        ImVec2 imgCoord = ImageCanvas::screenToImageCoord(
            io.MousePos, srcView.canvasPos, srcView.canvasSize,
            srcView.pan, srcView.zoom, srcView.baseScale,
            srcView.rotation, srcImg->width, srcImg->height);

        // Draw pixel coordinates src
        if (srcView.contains(io.MousePos))
        {
            dl->AddCircleFilled(io.MousePos, primaryRadius,
                                primaryFill);
            dl->AddCircle(io.MousePos, primaryRadius, primaryOutline,
                          12, 1.5f);
            snprintf(coordText, sizeof(coordText), "(%.1f, %.1f)",
                     imgCoord.x, imgCoord.y);
            ImVec2 textPos(io.MousePos.x + primaryRadius + 5.0f,
                           io.MousePos.y - primaryRadius);
            dl->AddText(textPos, primaryFill, coordText);
        }

        // apply transformation in image coordinates
        if (srcIsLeft)
        {
            transformed = align.transformLeft2Right(imgCoord);
        }
        else
        {  // src is Right-side image
            transformed = align.transformRight2Left(imgCoord);
        }

        // Convert to screen coordinates
        dstScreenPos = ImageCanvas::imageToScreenCoord(
            transformed.x, transformed.y, dstAdjustCenter.x,
            dstAdjustCenter.y, dstView.renderScale, dstView.rotation,
            dstImg->width, dstImg->height);

        // Check if transformed position is within dst screen bounds
        if (dstView.contains(dstScreenPos))
        {
            // Draw orange transformed cursor indicator
            dl->AddCircleFilled(dstScreenPos, transformedRadius,
                                transformedFill);
            dl->AddCircle(dstScreenPos, transformedRadius,
                          transformedOutline, 12, 1.5f);

            // Draw coordinate text below circle
            snprintf(transformedText, sizeof(transformedText),
                     "(%.1f, %.1f)", transformed.x, transformed.y);
            ImVec2 transformedTextPos(
                dstScreenPos.x - 30.0f,
                dstScreenPos.y + transformedRadius + 5.0f);
            dl->AddText(transformedTextPos, transformedTextColor,
                        transformedText);
        }

        // Find if cursor is near any matched point
        for (size_t i = 0; i < N; ++i)
        {
            srcX = srcPoints[i]["x"].getNumber();
            srcY = srcPoints[i]["y"].getNumber();
            srcScreenPos = ImageCanvas::imageToScreenCoord(
                srcX, srcY, srcAdjustCenter.x, srcAdjustCenter.y,
                srcView.renderScale, srcView.rotation, srcImg->width,
                srcImg->height);

            dstX = dstPoints[i]["x"].getNumber();
            dstY = dstPoints[i]["y"].getNumber();
            dstScreenPos = ImageCanvas::imageToScreenCoord(
                dstX, dstY, dstAdjustCenter.x, dstAdjustCenter.y,
                dstView.renderScale, dstView.rotation, dstImg->width,
                dstImg->height);

            if (!srcView.contains(srcScreenPos)) continue;
            if (!dstView.contains(dstScreenPos)) continue;

            // draw circle for src
            dl->AddCircleFilled(srcScreenPos, secondaryRadius,
                                correspondingColor);
            dl->AddCircle(srcScreenPos, secondaryRadius, primaryOutline,
                          12, 2.5f);

            // draw corresponding circle on dst
            dl->AddCircleFilled(dstScreenPos, secondaryRadius,
                                correspondingColor);
            dl->AddCircle(dstScreenPos, secondaryRadius, primaryOutline,
                          12, 2.5f);

            // Check if cursor is near this point
            ImVec2 d = io.MousePos - srcScreenPos;
            dist = sqrtf(d.x * d.x + d.y * d.y);

            if (dist < hoverThreshold)
            {
                // Draw line connecting the matched points
                dl->AddLine(srcScreenPos, dstScreenPos,
                            correspondingColor,
                            ImageCanvas::style.boundsLineThickness);
            }
        }
    }

    void ImageCanvas::renderLockedCursorIndicators(
        const ImageCanvas& left, const ImageCanvas& right,
        const AlignState& align, const SettingsState& settings)
    {
        if (!left.image || !right.image) { return; }

        // Guard against division by zero in inverse transformation
        if (align.scale < 0.001f) { return; }

        // Check if we have matched points stored
        if (!align.info.isObject() ||
            !align.info.contains("leftPoints") ||
            !align.info.contains("rightPoints"))
        {
            return;  // No matched points available
        }

        auto& leftPoints = align.info["leftPoints"].getArray();
        auto& rightPoints = align.info["rightPoints"].getArray();
        if (leftPoints.size() != rightPoints.size()) { return; }

        if (right.viewState.isHovered)
        {
            renderLockedCursorIndicators0(align,
                                          /*src=*/right,
                                          /*srcPoints=*/rightPoints,
                                          /*dst=*/left,
                                          /*dstPoints=*/leftPoints,
                                          false, settings);
        }
        else
        {
            renderLockedCursorIndicators0(align,
                                          /*src=*/left,
                                          /*srcPoints=*/leftPoints,
                                          /*dst=*/right,
                                          /*dstPoints=*/rightPoints,
                                          true, settings);
        }
    }

    ImageCanvas::ViewTargets ImageCanvas::snapshotTargets(
        const ImageCanvas& c)
    {
        ViewTargets t;
        t.zoom = c.viewState.zoomTarget;
        t.pan = c.viewState.panTarget;
        t.rotation = c.viewState.rotationTarget;
        return t;
    }

    void ImageCanvas::syncPair(ImageCanvas& left, ImageCanvas& right,
                               const ViewTargets& l0,
                               const ViewTargets& r0,
                               const AlignState& align, bool& locked,
                               bool alignDialogOpen,
                               const SettingsState& settings)
    {
        auto& lv = left.viewState;
        auto& rv = right.viewState;

        if (lv.homeRequested || rv.homeRequested)
        {
            lv.zoomTarget = 1.0f;
            lv.panTarget = ImVec2(0, 0);
            lv.rotationTarget = 0.0f;
            applyAlignment(left, right, align, locked);
        }
        else
        {
            syncLockedViewers(left, right, align, l0, r0);
        }

        if (!alignDialogOpen)
            renderLockedCursorIndicators(left, right, align, settings);

        lv.homeRequested = false;
        rv.homeRequested = false;
    }

} /* namespace shoecomp */
