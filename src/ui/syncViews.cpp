#include "ui/mainWindow.h"

namespace shoecomp
{

    // Apply alignment transform: set right viewer
    // to match left viewer + alignment offset.
    void applyAlignment(ImageCanvas& viewerLeft,
                        ImageCanvas& viewerRight, const AlignState& a,
                        bool& locked)
    {
        auto& lv = viewerLeft.viewState;
        auto& rv = viewerRight.viewState;

        if (!viewerLeft.image || !viewerRight.image)
        {
            locked = false;
            return;
        }

        locked = false;
        float aRad = a.rotation;
        rv.zoom = rv.zoomTarget = lv.zoomTarget * a.scale;
        rv.rotation = rv.rotationTarget = lv.rotationTarget + aRad;

        // Use each viewer's canvas center as reference point
        ImVec2 leftRefScreen(lv.canvasPos.x + lv.canvasSize.x * 0.5f,
                             lv.canvasPos.y + lv.canvasSize.y * 0.5f);
        ImVec2 rightRefScreen(rv.canvasPos.x + rv.canvasSize.x * 0.5f,
                              rv.canvasPos.y + rv.canvasSize.y * 0.5f);

        // Convert left reference to image coordinates
        ImVec2 leftImgPt = ImageCanvas::screenToImageCoord(
            leftRefScreen, lv.canvasPos, lv.canvasSize, lv.panTarget,
            lv.zoomTarget, lv.baseScale, lv.rotationTarget,
            viewerLeft.image->width, viewerLeft.image->height);

        // Apply alignment transformation from left to right
        ImVec2 rightImgPt = a.transformLeft2Right(leftImgPt);

        // Start with same pan as left, then compute adjustment
        rv.pan = rv.panTarget = lv.panTarget;

        // Compute right viewer rendering state
        float rightRenderScale = rv.baseScale * rv.zoomTarget;
        float rightCenterX =
            rv.canvasPos.x + rv.canvasSize.x * 0.5f + rv.panTarget.x;
        float rightCenterY =
            rv.canvasPos.y + rv.canvasSize.y * 0.5f + rv.panTarget.y;

        // Convert right image point back to screen coordinates
        ImVec2 rightScreen = ImageCanvas::imageToScreenCoord(
            rightImgPt.x, rightImgPt.y, rightCenterX, rightCenterY,
            rightRenderScale, cosf(rv.rotationTarget),
            sinf(rv.rotationTarget), viewerRight.image->width,
            viewerRight.image->height);

        // Adjust pan so right image point appears at right canvas
        // center
        rv.panTarget.x += rightRefScreen.x - rightScreen.x;
        rv.panTarget.y += rightRefScreen.y - rightScreen.y;
        rv.pan = rv.panTarget;

        locked = true;
    }

    // Propagate a zoom change on the left viewer to
    // the right viewer, keeping the transformed image
    // point under the user's cursor anchored to its
    // current right-canvas screen position.
    void propagateZoomLeftToRight(ImageCanvas& viewerLeft,
                                  ImageCanvas& viewerRight,
                                  const AlignState& a, float rZoom0,
                                  ImVec2 rPan0, float rRot0)
    {
        auto& lv = viewerLeft.viewState;
        auto& rv = viewerRight.viewState;

        ImVec2 cursor = ImGui::GetIO().MousePos;
        ImVec2 leftImgPt = ImageCanvas::screenToImageCoord(
            cursor, lv.canvasPos, lv.canvasSize, lv.panTarget,
            lv.zoomTarget, lv.baseScale, lv.rotationTarget,
            viewerLeft.image->width, viewerLeft.image->height);
        ImVec2 rightImgPt = a.transformLeft2Right(leftImgPt);

        float oldRightScale = rv.baseScale * rZoom0;
        float oldRightCx =
            rv.canvasPos.x + rv.canvasSize.x * 0.5f + rPan0.x;
        float oldRightCy =
            rv.canvasPos.y + rv.canvasSize.y * 0.5f + rPan0.y;
        ImVec2 oldRightScreen = ImageCanvas::imageToScreenCoord(
            rightImgPt.x, rightImgPt.y, oldRightCx, oldRightCy,
            oldRightScale, cosf(rRot0), sinf(rRot0),
            viewerRight.image->width, viewerRight.image->height);

        rv.zoomTarget = lv.zoomTarget * a.scale;
        rv.rotationTarget = lv.rotationTarget + a.rotation;

        rv.panTarget = rPan0;
        float newRightScale = rv.baseScale * rv.zoomTarget;
        float newRightCx =
            rv.canvasPos.x + rv.canvasSize.x * 0.5f + rv.panTarget.x;
        float newRightCy =
            rv.canvasPos.y + rv.canvasSize.y * 0.5f + rv.panTarget.y;
        ImVec2 newRightScreen = ImageCanvas::imageToScreenCoord(
            rightImgPt.x, rightImgPt.y, newRightCx, newRightCy,
            newRightScale, cosf(rv.rotationTarget),
            sinf(rv.rotationTarget), viewerRight.image->width,
            viewerRight.image->height);

        rv.panTarget.x += oldRightScreen.x - newRightScreen.x;
        rv.panTarget.y += oldRightScreen.y - newRightScreen.y;
    }

    // Mirror of propagateZoomLeftToRight for the
    // right-driven case.
    void propagateZoomRightToLeft(ImageCanvas& viewerLeft,
                                  ImageCanvas& viewerRight,
                                  const AlignState& a, float lZoom0,
                                  ImVec2 lPan0, float lRot0)
    {
        auto& lv = viewerLeft.viewState;
        auto& rv = viewerRight.viewState;

        ImVec2 cursor = ImGui::GetIO().MousePos;
        ImVec2 rightImgPt = ImageCanvas::screenToImageCoord(
            cursor, rv.canvasPos, rv.canvasSize, rv.panTarget,
            rv.zoomTarget, rv.baseScale, rv.rotationTarget,
            viewerRight.image->width, viewerRight.image->height);
        ImVec2 leftImgPt = a.transformRight2Left(rightImgPt);

        float oldLeftScale = lv.baseScale * lZoom0;
        float oldLeftCx =
            lv.canvasPos.x + lv.canvasSize.x * 0.5f + lPan0.x;
        float oldLeftCy =
            lv.canvasPos.y + lv.canvasSize.y * 0.5f + lPan0.y;
        ImVec2 oldLeftScreen = ImageCanvas::imageToScreenCoord(
            leftImgPt.x, leftImgPt.y, oldLeftCx, oldLeftCy,
            oldLeftScale, cosf(lRot0), sinf(lRot0),
            viewerLeft.image->width, viewerLeft.image->height);

        lv.zoomTarget = rv.zoomTarget / a.scale;
        lv.rotationTarget = rv.rotationTarget - a.rotation;

        lv.panTarget = lPan0;
        float newLeftScale = lv.baseScale * lv.zoomTarget;
        float newLeftCx =
            lv.canvasPos.x + lv.canvasSize.x * 0.5f + lv.panTarget.x;
        float newLeftCy =
            lv.canvasPos.y + lv.canvasSize.y * 0.5f + lv.panTarget.y;
        ImVec2 newLeftScreen = ImageCanvas::imageToScreenCoord(
            leftImgPt.x, leftImgPt.y, newLeftCx, newLeftCy,
            newLeftScale, cosf(lv.rotationTarget),
            sinf(lv.rotationTarget), viewerLeft.image->width,
            viewerLeft.image->height);

        lv.panTarget.x += oldLeftScreen.x - newLeftScreen.x;
        lv.panTarget.y += oldLeftScreen.y - newLeftScreen.y;
    }

    // Sync locked viewers after rendering. Detects
    // which viewer changed and propagates through
    // alignment.
    void syncLockedViewers(ImageCanvas& viewerLeft,
                           ImageCanvas& viewerRight,
                           const AlignState& a, float lZoom0,
                           ImVec2 lPan0, float lRot0, float rZoom0,
                           ImVec2 rPan0, float rRot0)
    {
        auto& lv = viewerLeft.viewState;
        auto& rv = viewerRight.viewState;

        if (!viewerLeft.image || !viewerRight.image) return;

        float aRad = a.rotation;
        bool lChanged =
            lv.zoomTarget != lZoom0 || lv.panTarget.x != lPan0.x ||
            lv.panTarget.y != lPan0.y || lv.rotationTarget != lRot0;
        bool rChanged =
            rv.zoomTarget != rZoom0 || rv.panTarget.x != rPan0.x ||
            rv.panTarget.y != rPan0.y || rv.rotationTarget != rRot0;

        bool lZoomed = lv.zoomTarget != lZoom0;
        bool rZoomed = rv.zoomTarget != rZoom0;

        if ((lChanged && !rChanged) || (lChanged && rChanged))
        {
            if (lZoomed)
            {
                propagateZoomLeftToRight(viewerLeft, viewerRight, a,
                                         rZoom0, rPan0, rRot0);
            }
            else
            {
                rv.zoomTarget = lv.zoomTarget * a.scale;
                rv.zoom = lv.zoom * a.scale;
                rv.panTarget.x += (lv.panTarget.x - lPan0.x);
                rv.panTarget.y += (lv.panTarget.y - lPan0.y);
                rv.rotationTarget = lv.rotationTarget + aRad;
                rv.rotation = lv.rotation + aRad;
            }
        }
        else if (rChanged && !lChanged)
        {
            if (rZoomed)
            {
                propagateZoomRightToLeft(viewerLeft, viewerRight, a,
                                         lZoom0, lPan0, lRot0);
            }
            else
            {
                lv.zoomTarget = rv.zoomTarget / a.scale;
                lv.zoom = rv.zoom / a.scale;
                lv.panTarget.x += (rv.panTarget.x - rPan0.x);
                lv.panTarget.y += (rv.panTarget.y - rPan0.y);
                lv.rotationTarget = rv.rotationTarget - aRad;
                lv.rotation = rv.rotation - aRad;
            }
        }
    }

    static void renderLockedCursorIndicators0(
        AppState& state, AlignState& align, ImageCanvas& src,
        std::vector<jt::Json>& srcPoints, ImageCanvas& dst,
        std::vector<jt::Json>& dstPoints, bool srcIsLeft,
        const SettingsState& settings)
    {
        // Visual constants (from settings)
        const float primaryRadius = settings.cursorRadius;
        const ImU32 primaryFill = ImGui::ColorConvertFloat4ToU32(
            ImVec4(settings.cursorColor[0], settings.cursorColor[1],
                   settings.cursorColor[2], settings.cursorColor[3]));
        const ImU32 primaryOutline = IM_COL32(255, 255, 255, 255);
        const float secondaryRadius = settings.correspondingRadius;
        const ImU32 correspondingColor = ImGui::ColorConvertFloat4ToU32(
            ImVec4(settings.correspondingColor[0],
                   settings.correspondingColor[1],
                   settings.correspondingColor[2],
                   settings.correspondingColor[3]));
        const float hoverThreshold = settings.hoverThreshold;

        // Transformed cursor indicator constants
        const float transformedRadius = settings.cursorRadius;
        const ImU32 transformedFill = ImGui::ColorConvertFloat4ToU32(
            ImVec4(settings.transformedColor[0],
                   settings.transformedColor[1],
                   settings.transformedColor[2],
                   settings.transformedColor[3]));
        const ImU32 transformedOutline =
            IM_COL32(255, 255, 255, 255);  // White
        const ImU32 transformedTextColor =
            ImGui::ColorConvertFloat4ToU32(
                ImVec4(settings.transformedColor[0],
                       settings.transformedColor[1],
                       settings.transformedColor[2], 1.0f));

        ImGuiIO& io = ImGui::GetIO();
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        ImVec2 transformed;
        char coordText[64];
        char transformedText[64];
        char matchedText[64];
        //

        auto& srcView = src.viewState;
        auto& srcImg = src.image;
        auto& dstView = dst.viewState;
        auto& dstImg = dst.image;
        const size_t N = srcPoints.size();

        // Get cursor position in image coordinates
        ImVec2 imgCoord = ImageCanvas::screenToImageCoord(
            io.MousePos, srcView.canvasPos, srcView.canvasSize,
            srcView.pan, srcView.zoom, srcView.baseScale,
            srcView.rotation, srcImg->width, srcImg->height);

        // Always draw cyan circle at cursor
        dl->AddCircleFilled(io.MousePos, primaryRadius, primaryFill);
        dl->AddCircle(io.MousePos, primaryRadius, primaryOutline, 12,
                      1.5f);

        // Draw pixel coordinates src
        snprintf(coordText, sizeof(coordText), "(%.1f, %.1f)",
                 imgCoord.x, imgCoord.y);
        ImVec2 textPos(io.MousePos.x + primaryRadius + 5.0f,
                       io.MousePos.y - primaryRadius);
        dl->AddText(textPos, primaryFill, coordText);

        // apply transformation in image coordinates
        if (srcIsLeft)
        {
            transformed = align.transformLeft2Right(imgCoord);
        }
        else { transformed = align.transformRight2Left(imgCoord); }

        // Convert to screen coordinates
        ImVec2 dstScreenPos = ImageCanvas::imageToScreenCoord(
            transformed.x, transformed.y, dstView.centerX,
            dstView.centerY, dstView.renderScale,
            cosf(dstView.rotation), sinf(dstView.rotation),
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
            float lx = srcPoints[i]["x"].getFloat();
            float ly = srcPoints[i]["y"].getFloat();

            // Convert src point to screen coordinates
            ImVec2 srcScreenPos = ImageCanvas::imageToScreenCoord(
                lx, ly, srcView.centerX, srcView.centerY,
                srcView.renderScale, cosf(srcView.rotation),
                sinf(srcView.rotation), srcImg->width, srcImg->height);

            // Check if cursor is near this point
            float dx = io.MousePos.x - srcScreenPos.x;
            float dy = io.MousePos.y - srcScreenPos.y;
            float dist = sqrtf(dx * dx + dy * dy);

            if (dist < hoverThreshold)
            {
                // Draw corresponding circle on dst
                float rx = dstPoints[i]["x"].getFloat();
                float ry = dstPoints[i]["y"].getFloat();

                ImVec2 dstScreenPos = ImageCanvas::imageToScreenCoord(
                    rx, ry, dstView.centerX, dstView.centerY,
                    dstView.renderScale, cosf(dstView.rotation),
                    sinf(dstView.rotation), dstImg->width,
                    dstImg->height);

                dl->AddCircleFilled(dstScreenPos, secondaryRadius,
                                    correspondingColor);
                dl->AddCircle(dstScreenPos, secondaryRadius,
                              primaryOutline, 12, 2.5f);

                // Draw coordinate text below green circle
                snprintf(matchedText, sizeof(matchedText),
                         "(%.1f, %.1f)", rx, ry);
                ImVec2 matchedTextPos(
                    dstScreenPos.x - 30.0f,
                    dstScreenPos.y + secondaryRadius + 5.0f);
                dl->AddText(matchedTextPos, correspondingColor,
                            matchedText);
                break;  // Only show first matching point
            }
        }
    }

    void renderLockedCursorIndicators(AppState& state,
                                      const SettingsState& settings)
    {
        if (state.viewerLeftIdx < 0 || state.viewerRightIdx < 0)
        {
            return;
        }

        auto& leftView = state.viewerLeft.viewState;
        auto& rightView = state.viewerRight.viewState;

        if (!leftView.isHovered && !rightView.isHovered) { return; }

        auto& leftImg = state.viewerLeft.image;
        auto& rightImg = state.viewerRight.image;
        if (!leftImg || !rightImg) { return; }

        auto& align = state.viewerAlignments[state.viewerAlignmentIdx];

        // Debug: print transformation parameters (only once)
        static bool printed = false;
        if (!printed)
        {
            printf(
                "Align params: rot=%.2f, tx=%.2f, ty=%.2f, "
                "scale=%.4f\n",
                align.rotation, align.translationX, align.translationY,
                align.scale);
            printed = true;
        }

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
        if (leftPoints.size() == 0 ||
            leftPoints.size() != rightPoints.size())
        {
            return;
        }

        if (leftView.isHovered)
        {
            renderLockedCursorIndicators0(state, align,
                                          /*src=*/state.viewerLeft,
                                          /*srcPoints=*/leftPoints,
                                          /*dst=*/state.viewerRight,
                                          /*dstPoints=*/rightPoints,
                                          true, settings);
        }
        else if (rightView.isHovered)
        {
            renderLockedCursorIndicators0(state, align,
                                          /*src=*/state.viewerRight,
                                          /*srcPoints=*/rightPoints,
                                          /*dst=*/state.viewerLeft,
                                          /*dstPoints=*/leftPoints,
                                          false, settings);
        }
    }

} /* namespace shoecomp */
