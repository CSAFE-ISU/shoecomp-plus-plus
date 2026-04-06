#include "ui/mainWindow.h"

namespace shoecomp
{
    static inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs)
    {
        return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y);
    }
    static inline ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs)
    {
        return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y);
    }

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
        rv.zoom = rv.zoomTarget = lv.zoomTarget * a.scale;
        rv.rotation = rv.rotationTarget = lv.rotationTarget + a.rotation;

        // Use each viewer's canvas center as reference point
        ImVec2 leftRefScreen = lv.center();
        ImVec2 rightRefScreen = rv.center();

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
        ImVec2 rightCenter = rv.center() + rv.panTarget;

        // Convert right image point back to screen coordinates
        ImVec2 rightScreen = ImageCanvas::imageToScreenCoord(
            rightImgPt.x, rightImgPt.y, rightCenter.x, rightCenter.y,
            rightRenderScale, rv.rotationTarget, viewerRight.image->width,
            viewerRight.image->height);

        // Adjust pan so right image point appears at right canvas
        // center
        rv.panTarget = rightRefScreen - rightScreen;
        rv.pan = rv.panTarget;

        locked = true;
    }

    static void propagateZoom(ImageCanvas& src, ImageCanvas& dst,
                              const AlignState& a, bool srcIsLeft,
                              float zoom0, ImVec2 pan0, float rot0)
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
        srcImgPt = ImageCanvas::screenToImageCoord(
            cursor, srcView.canvasPos, srcView.canvasSize,
            srcView.panTarget, srcView.zoomTarget, srcView.baseScale,
            srcView.rotationTarget, src.image->width,
            src.image->height);

        oldDstScale = dstView.baseScale * zoom0;
        oldDstCenter = dstView.center() + pan0;

        if (srcIsLeft)
        {
            dstImgPt = a.transformLeft2Right(srcImgPt);
            dstView.zoomTarget = srcView.zoomTarget * a.scale;
            dstView.rotationTarget =
                srcView.rotationTarget + a.rotation;
            dstView.panTarget = pan0;
        }
        else
        {
            dstImgPt = a.transformRight2Left(srcImgPt);
            dstView.zoomTarget = srcView.zoomTarget / a.scale;
            dstView.rotationTarget =
                srcView.rotationTarget - a.rotation;
            dstView.panTarget = pan0;
        }

        oldDstScreen = ImageCanvas::imageToScreenCoord(
            dstImgPt.x, dstImgPt.y, oldDstCenter.x, oldDstCenter.y, oldDstScale,
            rot0, dst.image->width,
            dst.image->height);
        //
        newDstScale = dstView.baseScale * dstView.zoomTarget;
        newDstCenter = dstView.center() + pan0;
        newDstScreen = ImageCanvas::imageToScreenCoord(
            dstImgPt.x, dstImgPt.y, newDstCenter.x, newDstCenter.y, newDstScale,
            dstView.rotationTarget,
            dst.image->width, dst.image->height);

        dstView.panTarget = dstView.panTarget + (oldDstScreen - newDstScreen);
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
                propagateZoom(/*src=*/viewerLeft,
                              /*dst=*/viewerRight,
                              /*align*/ a, /*srcIsLeft*/ true,  //
                              rZoom0, rPan0, rRot0);
            }
            else
            {
                rv.zoomTarget = lv.zoomTarget * a.scale;
                rv.zoom = lv.zoom * a.scale;
                rv.panTarget = rv.panTarget + (lv.panTarget - lPan0);
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
                              lZoom0, lPan0, lRot0);
            }
            else
            {
                lv.zoomTarget = rv.zoomTarget / a.scale;
                lv.zoom = rv.zoom / a.scale;
                lv.panTarget = lv.panTarget + (rv.panTarget - rPan0);
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
            dstView.rotation,
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
                srcView.renderScale, srcView.rotation,
                srcImg->width, srcImg->height);

            // Check if cursor is near this point
            ImVec2 d = io.MousePos - srcScreenPos;
            float dist = sqrtf(d.x * d.x + d.y * d.y);

            if (dist < hoverThreshold)
            {
                // Draw corresponding circle on dst
                float rx = dstPoints[i]["x"].getFloat();
                float ry = dstPoints[i]["y"].getFloat();

                ImVec2 dstScreenPos = ImageCanvas::imageToScreenCoord(
                    rx, ry, dstView.centerX, dstView.centerY,
                    dstView.renderScale, dstView.rotation,
                    dstImg->width,
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
                align.rotation, align.dx, align.dy,
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
