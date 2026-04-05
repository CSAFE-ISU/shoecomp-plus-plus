#include "ui/mainWindow.h"
#include "ui/imageCanvas.h"
#include "ui/alignDialog.h"
#include "ui/uiHelpers.h"
#include "formats/png.h"
#include "formats/annotationIo.h"
#include "jtjson/json.h"
#include "hello_imgui/hello_imgui_include_opengl.h"
#include "hello_imgui/imgui_theme.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>

namespace shoecomp
{
    namespace fs = std::filesystem;

    static constexpr float kDegToRad = 3.14159265358979f / 180.0f;

    // Apply alignment transform: set right viewer
    // to match left viewer + alignment offset.
    static void applyAlignment(ImageCanvas& viewerLeft, ImageCanvas& viewerRight,
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
        float aRad = a.rotation * kDegToRad;
        rv.zoom = rv.zoomTarget = lv.zoomTarget * a.scale;
        rv.rotation = rv.rotationTarget = lv.rotationTarget + aRad;

        // Use each viewer's canvas center as reference point
        ImVec2 leftRefScreen(lv.canvasPos.x + lv.canvasSize.x * 0.5f,
                             lv.canvasPos.y + lv.canvasSize.y * 0.5f);
        ImVec2 rightRefScreen(rv.canvasPos.x + rv.canvasSize.x * 0.5f,
                              rv.canvasPos.y + rv.canvasSize.y * 0.5f);

        // Convert left reference to image coordinates
        ImVec2 leftImgPt = ImageCanvas::screenToImageCoord(
            leftRefScreen, lv.canvasPos, lv.canvasSize, lv.panTarget, lv.zoomTarget,
            lv.baseScale, lv.rotationTarget,
            viewerLeft.image->width, viewerLeft.image->height);

        // Apply alignment transformation: right = S * R * left + T
        float cosA = cosf(aRad);
        float sinA = sinf(aRad);
        float rotX = leftImgPt.x * cosA - leftImgPt.y * sinA;
        float rotY = leftImgPt.x * sinA + leftImgPt.y * cosA;
        ImVec2 rightImgPt(rotX * a.scale + a.translationX,
                          rotY * a.scale + a.translationY);

        // Start with same pan as left, then compute adjustment
        rv.pan = rv.panTarget = lv.panTarget;

        // Compute right viewer rendering state
        float rightRenderScale = rv.baseScale * rv.zoomTarget;
        float rightCenterX = rv.canvasPos.x + rv.canvasSize.x * 0.5f + rv.panTarget.x;
        float rightCenterY = rv.canvasPos.y + rv.canvasSize.y * 0.5f + rv.panTarget.y;

        // Convert right image point back to screen coordinates
        ImVec2 rightScreen = ImageCanvas::imageToScreenCoord(
            rightImgPt.x, rightImgPt.y, rightCenterX, rightCenterY, rightRenderScale,
            cosf(rv.rotationTarget), sinf(rv.rotationTarget),
            viewerRight.image->width, viewerRight.image->height);

        // Adjust pan so right image point appears at right canvas center
        rv.panTarget.x += rightRefScreen.x - rightScreen.x;
        rv.panTarget.y += rightRefScreen.y - rightScreen.y;
        rv.pan = rv.panTarget;

        locked = true;
    }

    // Sync locked viewers after rendering. Detects
    // which viewer changed and propagates through
    // alignment.
    static void syncLockedViewers(ImageCanvas& viewerLeft,
                                  ImageCanvas& viewerRight,
                                  const AlignState& a, float lZoom0,
                                  ImVec2 lPan0, float lRot0,
                                  float rZoom0, ImVec2 rPan0,
                                  float rRot0)
    {
        auto& lv = viewerLeft.viewState;
        auto& rv = viewerRight.viewState;

        if (!viewerLeft.image || !viewerRight.image)
            return;

        float aRad = a.rotation * kDegToRad;
        bool lChanged =
            lv.zoomTarget != lZoom0 || lv.panTarget.x != lPan0.x ||
            lv.panTarget.y != lPan0.y || lv.rotationTarget != lRot0;
        bool rChanged =
            rv.zoomTarget != rZoom0 || rv.panTarget.x != rPan0.x ||
            rv.panTarget.y != rPan0.y || rv.rotationTarget != rRot0;

        if (lChanged && !rChanged)
        {
            rv.zoomTarget = lv.zoomTarget * a.scale;
            rv.zoom = lv.zoom * a.scale;
            rv.panTarget.x += (lv.panTarget.x - lPan0.x);
            rv.panTarget.y += (lv.panTarget.y - lPan0.y);
            rv.rotationTarget = lv.rotationTarget + aRad;
            rv.rotation = lv.rotation + aRad;
        }
        else if (rChanged && !lChanged)
        {
            lv.zoomTarget = rv.zoomTarget / a.scale;
            lv.zoom = rv.zoom / a.scale;
            lv.panTarget.x += (rv.panTarget.x - rPan0.x);
            lv.panTarget.y += (rv.panTarget.y - rPan0.y);
            lv.rotationTarget = rv.rotationTarget - aRad;
            lv.rotation = rv.rotation - aRad;
        }
        else if (lChanged && rChanged)
        {
            rv.zoomTarget = lv.zoomTarget * a.scale;
            rv.zoom = lv.zoom * a.scale;
            rv.panTarget.x += (lv.panTarget.x - lPan0.x);
            rv.panTarget.y += (lv.panTarget.y - lPan0.y);
            rv.rotationTarget = lv.rotationTarget + aRad;
            rv.rotation = lv.rotation + aRad;
        }
    }

    static void runSplash(double duration)
    {
        double startTime = 0.0;

        HelloImGui::RunnerParams params;
        params.appWindowParams.windowTitle = "ShoeComp";
        params.appWindowParams.windowGeometry.size = {640, 360};
        params.appWindowParams.borderless = true;
        params.appWindowParams.borderlessMovable = false;
        params.appWindowParams.borderlessResizable = false;
        params.appWindowParams.borderlessClosable = false;
        params.appWindowParams.resizable = false;
        params.appWindowParams.windowGeometry.positionMode =
            HelloImGui::WindowPositionMode::MonitorCenter;
        params.imGuiWindowParams.defaultImGuiWindowType =
            HelloImGui::DefaultImGuiWindowType::ProvideFullScreenWindow;
        params.callbacks.PostInit = []()
        { ImGui::GetIO().FontGlobalScale = 2.5f; };
        params.callbacks.ShowGui = [&startTime, duration]()
        {
            if (startTime == 0.0) startTime = ImGui::GetTime();

            double elapsed = ImGui::GetTime() - startTime;

            ImVec2 winSize = ImGui::GetWindowSize();

            const char* title = "ShoeComp";
            ImVec2 titleSize = ImGui::CalcTextSize(title);
            ImGui::SetCursorPos(
                ImVec2((winSize.x - titleSize.x) * 0.5f,
                       (winSize.y - titleSize.y) * 0.5f - 30.0f));
            ImGui::Text("%s", title);

            // Animated dots: cycle 1-3
            int dots = (int)(elapsed / 0.4) % 3 + 1;
            char subtitle[16];
            snprintf(subtitle, sizeof(subtitle), "Loading%.*s", dots,
                     "...");
            // Use fixed width so text doesn't shift
            const char* widest = "Loading...";
            ImVec2 wSize = ImGui::CalcTextSize(widest);
            ImGui::SetCursorPos(
                ImVec2((winSize.x - wSize.x) * 0.5f,
                       (winSize.y - wSize.y) * 0.5f + 30.0f));
            ImGui::Text("%s", subtitle);

            if (elapsed >= duration)
                HelloImGui::GetRunnerParams()->appShallExit = true;
        };

        HelloImGui::Run(params);
    }

    static void renderSettings(AppState& state)
    {
        ImGui::Text("Loaded images:");
        int removeIdx = -1;
        for (int i = 0; i < (int)state.images.size(); ++i)
        {
            ImGui::PushID(i);
            if (ImGui::Button("X")) removeIdx = i;
            ImGui::SameLine();
            ImGui::Text("%s", state.images[i].image->name.c_str());
            ImGui::PopID();
        }
        if (removeIdx >= 0)
        {
            state.images.erase(state.images.begin() + removeIdx);
            int dummy = -1;
            clampViewerIndices(removeIdx, (int)state.images.size(),
                               state.viewerLeftIdx,
                               state.viewerRightIdx, dummy);
        }
    }

    static void renderFilesAndSettings(AppState& state)
    {
        struct SettingsState
        {
            int themeIdx = 1;
            bool fullscreen = true;
            float fontScale = 2.5f;
        };
        static SettingsState s;

        ImVec2 avail = ImGui::GetContentRegionAvail();
        float settingsH = avail.y * (2.0f / 3.0f);
        float imagesH = avail.y - settingsH;

        ImGui::BeginChild("SettingsPane", ImVec2(avail.x, settingsH),
                          ImGuiChildFlags_Borders);
        if (ImGui::BeginTable("##settings", 3))
        {
            ImGui::TableSetupColumn(
                "Label", ImGuiTableColumnFlags_WidthFixed, 250.0f);
            ImGui::TableSetupColumn(
                "Spacer", ImGuiTableColumnFlags_WidthFixed, 20.0f);
            ImGui::TableSetupColumn("Widget",
                                    ImGuiTableColumnFlags_WidthStretch);

            settingsTableRow("Theme");
            ImGui::Combo("##Theme", &s.themeIdx, "Light\0Dark\0");

            settingsTableRow("Fullscreen");
            ImGui::Checkbox("##Fullscreen", &s.fullscreen);

            settingsTableRow("Font Scale");
            ImGui::SliderFloat("##FontScale", &s.fontScale, 0.5f, 4.0f,
                               "%.2f");
            s.fontScale = std::round(s.fontScale / 0.05f) * 0.05f;

            ImGui::EndTable();
        }
        ImGui::Spacing();
        ImGui::SeparatorText("Annotations");
        if (ImGui::BeginTable("##annSettings", 3))
        {
            ImGui::TableSetupColumn(
                "Label", ImGuiTableColumnFlags_WidthFixed, 250.0f);
            ImGui::TableSetupColumn(
                "Spacer", ImGuiTableColumnFlags_WidthFixed, 20.0f);
            ImGui::TableSetupColumn("Widget",
                                    ImGuiTableColumnFlags_WidthStretch);

            settingsTableRow("Point Radius");
            ImGui::SliderFloat("##PointRadius",
                               &g_annotationStyle.pointRadius, 2.0f,
                               15.0f, "%.1f");

            settingsTableRow("Corner Color");
            ImGui::ColorEdit4("##CornerColor",
                              g_annotationStyle.cornerColor);

            settingsTableRow("Center Color");
            ImGui::ColorEdit4("##CenterColor",
                              g_annotationStyle.centerColor);

            settingsTableRow("Bounds Thickness");
            ImGui::SliderFloat("##BoundsThickness",
                               &g_annotationStyle.boundsLineThickness,
                               1.0f, 8.0f, "%.1f");

            settingsTableRow("Bounds Color");
            ImGui::ColorEdit4("##BoundsColor",
                              g_annotationStyle.boundsColor);

            ImGui::EndTable();
        }

        ImGui::Spacing();
        if (ImGui::Button("Update Settings"))
        {
            auto theme = s.themeIdx == 0
                             ? ImGuiTheme::ImGuiTheme_ImGuiColorsLight
                             : ImGuiTheme::ImGuiTheme_ImGuiColorsDark;
            ImGuiTheme::ApplyTheme(theme);
            HelloImGui::GetRunnerParams()
                ->appWindowParams.windowGeometry.fullScreenMode =
                s.fullscreen
                    ? HelloImGui::FullScreenMode::FullMonitorWorkArea
                    : HelloImGui::FullScreenMode::NoFullScreen;
            ImGui::GetIO().FontGlobalScale = s.fontScale;
        }
        ImGui::EndChild();

        ImGui::BeginChild("LoadedImagesPane", ImVec2(avail.x, imagesH),
                          ImGuiChildFlags_Borders);
        renderSettings(state);
        ImGui::EndChild();
    }

    static void renderLockedCursorIndicators(AppState& state)
    {
        // Debug: check each condition
        if (!state.viewerLocked)
        {
            // Not locked - this is expected, no debug needed
            return;
        }

        if (state.viewerLeftIdx < 0 || state.viewerRightIdx < 0)
        {
            // Debug: uncomment to see if indices are invalid
            // printf("DEBUG: Invalid indices - left: %d, right: %d\n",
            //        state.viewerLeftIdx, state.viewerRightIdx);
            return;
        }

        auto& leftView = state.viewerLeft.viewState;
        auto& rightView = state.viewerRight.viewState;

        if (!leftView.isHovered && !rightView.isHovered)
        {
            // Debug: uncomment to see hover state
            // printf("DEBUG: Neither hovered - left: %d, right: %d\n",
            //        leftView.isHovered, rightView.isHovered);
            return;
        }

        auto& leftImg = state.viewerLeft.image;
        auto& rightImg = state.viewerRight.image;
        if (!leftImg || !rightImg)
        {
            // Debug: uncomment to see if images are missing
            // printf("DEBUG: Missing images - left: %p, right: %p\n",
            //        (void*)leftImg.get(), (void*)rightImg.get());
            return;
        }

        // Debug: uncomment to confirm we're reaching the drawing code
        // printf("DEBUG: Drawing circles! Left hovered: %d, Right hovered: %d\n",
        //        leftView.isHovered, rightView.isHovered);

        auto& align = state.viewerAlignments[state.viewerAlignmentIdx];

        // Guard against division by zero in inverse transformation
        if (align.scale < 0.001f)
        {
            return;
        }

        // Check if we have matched points stored
        if (!align.info.isObject() ||
            !align.info.contains("leftPoints") ||
            !align.info.contains("rightPoints"))
        {
            return;  // No matched points available
        }

        auto& leftPoints = align.info["leftPoints"].getArray();
        auto& rightPoints = align.info["rightPoints"].getArray();
        if (leftPoints.size() == 0 || leftPoints.size() != rightPoints.size())
        {
            return;
        }

        ImGuiIO& io = ImGui::GetIO();
        ImDrawList* dl = ImGui::GetForegroundDrawList();

        // Visual constants
        const float primaryRadius = 8.0f;
        const ImU32 primaryFill = IM_COL32(0, 220, 255, 200);
        const ImU32 primaryOutline = IM_COL32(255, 255, 255, 255);
        const float secondaryRadius = 8.0f;
        const ImU32 correspondingColor = IM_COL32(0, 255, 100, 180);  // Green: corresponding point
        const float hoverThreshold = 15.0f;  // pixels

        // Transformed cursor indicator constants
        const float transformedRadius = 7.0f;
        const ImU32 transformedFill = IM_COL32(255, 150, 0, 180);      // Orange
        const ImU32 transformedOutline = IM_COL32(255, 255, 255, 255);  // White
        const ImU32 transformedTextColor = IM_COL32(255, 150, 0, 255);  // Orange (opaque)

        if (leftView.isHovered)
        {
            // Get cursor position in image coordinates
            ImVec2 imgCoord = ImageCanvas::screenToImageCoord(
                io.MousePos, leftView.canvasPos, leftView.canvasSize,
                leftView.pan, leftView.zoom, leftView.baseScale,
                leftView.rotation, leftImg->width, leftImg->height);

            // Always draw cyan circle at cursor
            dl->AddCircleFilled(io.MousePos, primaryRadius, primaryFill);
            dl->AddCircle(io.MousePos, primaryRadius, primaryOutline, 12, 1.5f);

            // Draw pixel coordinates
            char coordText[64];
            snprintf(coordText, sizeof(coordText), "(%.1f, %.1f)",
                     imgCoord.x, imgCoord.y);
            ImVec2 textPos(io.MousePos.x + primaryRadius + 5.0f,
                          io.MousePos.y - primaryRadius);
            dl->AddText(textPos, primaryFill, coordText);

            // Apply inverse RTS transformation: left -> right
            // Inverse of (Rotate, Scale, Translate) is: Untranslate, Unrotate, Unscale
            float radians = align.rotation * 3.14159265f / 180.0f;
            float cos_theta = cosf(radians);
            float sin_theta = sinf(radians);

            float tempX = imgCoord.x - align.translationX;
            float tempY = imgCoord.y - align.translationY;
            float rotX = tempX * cos_theta + tempY * sin_theta;
            float rotY = -tempX * sin_theta + tempY * cos_theta;
            float transformedX = rotX / align.scale;
            float transformedY = rotY / align.scale;

            // Check if transformed position is within right image bounds
            if (transformedX >= 0 && transformedX < rightImg->width &&
                transformedY >= 0 && transformedY < rightImg->height)
            {
                // Convert to screen coordinates
                ImVec2 rightScreenPos = ImageCanvas::imageToScreenCoord(
                    transformedX, transformedY,
                    rightView.centerX, rightView.centerY,
                    rightView.renderScale,
                    cosf(rightView.rotation), sinf(rightView.rotation),
                    rightImg->width, rightImg->height);

                // Draw orange transformed cursor indicator
                dl->AddCircleFilled(rightScreenPos, transformedRadius,
                                  transformedFill);
                dl->AddCircle(rightScreenPos, transformedRadius,
                            transformedOutline, 12, 1.5f);

                // Draw coordinate text below circle
                char transformedText[64];
                snprintf(transformedText, sizeof(transformedText),
                        "(%.1f, %.1f)", transformedX, transformedY);
                ImVec2 transformedTextPos(rightScreenPos.x - 30.0f,
                                        rightScreenPos.y + transformedRadius + 5.0f);
                dl->AddText(transformedTextPos, transformedTextColor,
                          transformedText);
            }

            // Find if cursor is near any matched point
            for (size_t i = 0; i < leftPoints.size(); ++i)
            {
                float lx = leftPoints[i]["x"].getFloat();
                float ly = leftPoints[i]["y"].getFloat();

                // Convert left point to screen coordinates
                ImVec2 leftScreenPos = ImageCanvas::imageToScreenCoord(
                    lx, ly, leftView.centerX, leftView.centerY,
                    leftView.renderScale,
                    cosf(leftView.rotation), sinf(leftView.rotation),
                    leftImg->width, leftImg->height);

                // Check if cursor is near this point
                float dx = io.MousePos.x - leftScreenPos.x;
                float dy = io.MousePos.y - leftScreenPos.y;
                float dist = sqrtf(dx * dx + dy * dy);

                if (dist < hoverThreshold)
                {
                    // Draw corresponding circle on right
                    float rx = rightPoints[i]["x"].getFloat();
                    float ry = rightPoints[i]["y"].getFloat();

                    ImVec2 rightScreenPos = ImageCanvas::imageToScreenCoord(
                        rx, ry, rightView.centerX, rightView.centerY,
                        rightView.renderScale,
                        cosf(rightView.rotation), sinf(rightView.rotation),
                        rightImg->width, rightImg->height);

                    dl->AddCircleFilled(rightScreenPos, secondaryRadius,
                                      correspondingColor);
                    dl->AddCircle(rightScreenPos, secondaryRadius,
                                primaryOutline, 12, 2.5f);

                    // Draw coordinate text below green circle
                    char matchedText[64];
                    snprintf(matchedText, sizeof(matchedText),
                            "(%.1f, %.1f)", rx, ry);
                    ImVec2 matchedTextPos(rightScreenPos.x - 30.0f,
                                        rightScreenPos.y + secondaryRadius + 5.0f);
                    dl->AddText(matchedTextPos, correspondingColor, matchedText);
                    break;  // Only show first matching point
                }
            }
        }
        else if (rightView.isHovered)
        {
            // Get cursor position in image coordinates
            ImVec2 imgCoord = ImageCanvas::screenToImageCoord(
                io.MousePos, rightView.canvasPos, rightView.canvasSize,
                rightView.pan, rightView.zoom, rightView.baseScale,
                rightView.rotation, rightImg->width, rightImg->height);

            // Always draw cyan circle at cursor
            dl->AddCircleFilled(io.MousePos, primaryRadius, primaryFill);
            dl->AddCircle(io.MousePos, primaryRadius, primaryOutline, 12, 1.5f);

            // Draw pixel coordinates
            char coordText[64];
            snprintf(coordText, sizeof(coordText), "(%.1f, %.1f)",
                     imgCoord.x, imgCoord.y);
            ImVec2 textPos(io.MousePos.x + primaryRadius + 5.0f,
                          io.MousePos.y - primaryRadius);
            dl->AddText(textPos, primaryFill, coordText);

            // Apply forward RTS transformation: right -> left
            // Scale, Rotate, Translate
            float radians = align.rotation * 3.14159265f / 180.0f;
            float cos_theta = cosf(radians);
            float sin_theta = sinf(radians);

            float scaledX = imgCoord.x * align.scale;
            float scaledY = imgCoord.y * align.scale;
            float rotX = scaledX * cos_theta - scaledY * sin_theta;
            float rotY = scaledX * sin_theta + scaledY * cos_theta;
            float transformedX = rotX + align.translationX;
            float transformedY = rotY + align.translationY;

            // Check if transformed position is within left image bounds
            if (transformedX >= 0 && transformedX < leftImg->width &&
                transformedY >= 0 && transformedY < leftImg->height)
            {
                // Convert to screen coordinates
                ImVec2 leftScreenPos = ImageCanvas::imageToScreenCoord(
                    transformedX, transformedY,
                    leftView.centerX, leftView.centerY,
                    leftView.renderScale,
                    cosf(leftView.rotation), sinf(leftView.rotation),
                    leftImg->width, leftImg->height);

                // Draw orange transformed cursor indicator
                dl->AddCircleFilled(leftScreenPos, transformedRadius,
                                  transformedFill);
                dl->AddCircle(leftScreenPos, transformedRadius,
                            transformedOutline, 12, 1.5f);

                // Draw coordinate text below circle
                char transformedText[64];
                snprintf(transformedText, sizeof(transformedText),
                        "(%.1f, %.1f)", transformedX, transformedY);
                ImVec2 transformedTextPos(leftScreenPos.x - 30.0f,
                                        leftScreenPos.y + transformedRadius + 5.0f);
                dl->AddText(transformedTextPos, transformedTextColor,
                          transformedText);
            }

            // Find if cursor is near any matched point
            for (size_t i = 0; i < rightPoints.size(); ++i)
            {
                float rx = rightPoints[i]["x"].getFloat();
                float ry = rightPoints[i]["y"].getFloat();

                // Convert right point to screen coordinates
                ImVec2 rightScreenPos = ImageCanvas::imageToScreenCoord(
                    rx, ry, rightView.centerX, rightView.centerY,
                    rightView.renderScale,
                    cosf(rightView.rotation), sinf(rightView.rotation),
                    rightImg->width, rightImg->height);

                // Check if cursor is near this point
                float dx = io.MousePos.x - rightScreenPos.x;
                float dy = io.MousePos.y - rightScreenPos.y;
                float dist = sqrtf(dx * dx + dy * dy);

                if (dist < hoverThreshold)
                {
                    // Draw corresponding circle on left
                    float lx = leftPoints[i]["x"].getFloat();
                    float ly = leftPoints[i]["y"].getFloat();

                    ImVec2 leftScreenPos = ImageCanvas::imageToScreenCoord(
                        lx, ly, leftView.centerX, leftView.centerY,
                        leftView.renderScale,
                        cosf(leftView.rotation), sinf(leftView.rotation),
                        leftImg->width, leftImg->height);

                    dl->AddCircleFilled(leftScreenPos, secondaryRadius,
                                      correspondingColor);
                    dl->AddCircle(leftScreenPos, secondaryRadius,
                                primaryOutline, 12, 2.5f);

                    // Draw coordinate text below green circle
                    char matchedText[64];
                    snprintf(matchedText, sizeof(matchedText),
                            "(%.1f, %.1f)", lx, ly);
                    ImVec2 matchedTextPos(leftScreenPos.x - 30.0f,
                                        leftScreenPos.y + secondaryRadius + 5.0f);
                    dl->AddText(matchedTextPos, correspondingColor, matchedText);
                    break;  // Only show first matching point
                }
            }
        }
    }

    static void renderLockToggle(bool& locked)
    {
        if (ImGui::Button(locked ? "Unlock" : "Lock")) locked = !locked;
    }

    static void renderSingleViewer(AppState& state, int& selectedIdx,
                                   int otherIdx, ImageCanvas& viewer,
                                   const char* label,
                                   ImageViewState* linked = nullptr)
    {
        const char* preview =
            (selectedIdx >= 0 && selectedIdx < (int)state.images.size())
                ? state.images[selectedIdx].image->name.c_str()
                : "<none>";

        if (ImGui::BeginCombo(label, preview))
        {
            if (ImGui::Selectable("<none>", selectedIdx < 0))
            {
                selectedIdx = -1;
                viewer.viewState.zoom = viewer.viewState.zoomTarget =
                    1.0f;
                viewer.viewState.pan = viewer.viewState.panTarget =
                    ImVec2(0, 0);
            }
            for (int i = 0; i < (int)state.images.size(); ++i)
            {
                if (i == otherIdx) continue;
                bool selected = (i == selectedIdx);
                if (ImGui::Selectable(
                        state.images[i].image->name.c_str(), selected))
                {
                    selectedIdx = i;
                    viewer.viewState.zoom =
                        viewer.viewState.zoomTarget = 0.0f;
                    viewer.viewState.pan = viewer.viewState.panTarget =
                        ImVec2(0, 0);
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (selectedIdx < 0 || selectedIdx >= (int)state.images.size())
            return;

        viewer.image = state.images[selectedIdx].image;
        float toolbarH = ImGui::GetFrameHeightWithSpacing() * 3.0f;
        ImVec2 region = ImGui::GetContentRegionAvail();
        float canvasH = region.y - toolbarH;
        if (canvasH > 0.0f)
        {
            ImGui::BeginChild("##cvs", ImVec2(0, canvasH),
                              ImGuiChildFlags_None);
            viewer.renderCanvas("##canvas", linked);
            ImGui::EndChild();
        }
        viewer.renderToolbar(label, linked);
    }

    static void renderImageViewer(AppState& state)
    {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float dockH = ImGui::GetFrameHeightWithSpacing() +
                      ImGui::GetStyle().ItemSpacing.y;
        float contentH = avail.y - dockH;

        ImGui::BeginChild("ComparisonContent",
                          ImVec2(avail.x, contentH),
                          ImGuiChildFlags_None);
        {
            float totalW = ImGui::GetContentRegionAvail().x;
            float splitterW = 8.0f;
            float leftW =
                totalW * state.viewerSplitRatio - splitterW * 0.5f;
            float rightW = totalW * (1.0f - state.viewerSplitRatio) -
                           splitterW * 0.5f;

            // Snapshot targets before rendering so
            // we can detect which viewer changed.
            auto& lv = state.viewerLeft.viewState;
            auto& rv = state.viewerRight.viewState;

            float lZoom0 = lv.zoomTarget;
            ImVec2 lPan0 = lv.panTarget;
            float lRot0 = lv.rotationTarget;
            float rZoom0 = rv.zoomTarget;
            ImVec2 rPan0 = rv.panTarget;
            float rRot0 = rv.rotationTarget;

            ImGui::BeginChild("LeftViewer", ImVec2(leftW, 0),
                              ImGuiChildFlags_Borders);
            renderSingleViewer(state, state.viewerLeftIdx,
                               state.viewerRightIdx, state.viewerLeft,
                               "##Left");
            ImGui::EndChild();

            ImGui::SameLine();

            // Draggable splitter
            float height = ImGui::GetContentRegionAvail().y;
            ImGui::Button("##Splitter", ImVec2(splitterW, height));
            if (ImGui::IsItemActive())
            {
                float delta = ImGui::GetIO().MouseDelta.x;
                state.viewerSplitRatio += delta / totalW;
                state.viewerSplitRatio =
                    std::clamp(state.viewerSplitRatio, 0.1f, 0.9f);
            }
            if (ImGui::IsItemHovered() || ImGui::IsItemActive())
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

            ImGui::SameLine();

            ImGui::BeginChild("RightViewer", ImVec2(rightW, 0),
                              ImGuiChildFlags_Borders);
            renderSingleViewer(state, state.viewerRightIdx,
                               state.viewerLeftIdx, state.viewerRight,
                               "##Right");
            ImGui::EndChild();

            // Draw locked cursor indicators
            renderLockedCursorIndicators(state);

            // Apply locked sync with alignment
            // offset after both viewers render.
            if (state.viewerLocked)
            {
                auto& a =
                    state.viewerAlignments[state.viewerAlignmentIdx];
                syncLockedViewers(state.viewerLeft, state.viewerRight, a,
                                  lZoom0, lPan0, lRot0,
                                  rZoom0, rPan0, rRot0);
            }
        }
        ImGui::EndChild();

        // --- Dock bar ---
        ImGui::Separator();
        bool hasLeft = state.viewerLeftIdx >= 0 &&
                       state.viewerLeftIdx < (int)state.images.size();
        bool hasRight = state.viewerRightIdx >= 0 &&
                        state.viewerRightIdx < (int)state.images.size();
        bool hasBoth = hasLeft && hasRight;
        ImGui::BeginDisabled(!hasBoth);
        if (ImGui::Button(state.viewerLocked ? "Unlock" : "Lock"))
            state.viewerLocked = !state.viewerLocked;
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(!hasBoth);
        if (ImGui::Button("Align"))
        {
            state.alignDialog.show = true;
            state.alignDialog.leftName =
                state.images[state.viewerLeftIdx].image->name;
            state.alignDialog.rightName =
                state.images[state.viewerRightIdx].image->name;
            state.alignDialog.leftImage =
                state.images[state.viewerLeftIdx].image;
            state.alignDialog.rightImage =
                state.images[state.viewerRightIdx].image;
            state.viewerLocked = false;
            auto& lv = state.viewerLeft.viewState;
            lv.zoom = lv.zoomTarget = 1.0f;
            lv.pan = lv.panTarget = ImVec2(0, 0);
            lv.rotation = lv.rotationTarget = 0.0f;
            auto& rv = state.viewerRight.viewState;
            rv.zoom = rv.zoomTarget = 1.0f;
            rv.pan = rv.panTarget = ImVec2(0, 0);
            rv.rotation = rv.rotationTarget = 0.0f;
            state.viewerLocked = true;
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(!state.viewerLocked);
        if (ImGui::Button("Save JSON"))
        {
            state.alignmentSaveBrowser.show = true;
            state.alignmentSaveBrowser.dirNeedsRefresh = true;
            state.alignmentSaveBrowser.fileName.clear();
        }
        ImGui::EndDisabled();

        if (state.viewerLocked)
        {
            ImGui::SameLine();
            bool navDisabled = (int)state.viewerAlignments.size() <= 1;
            if (navDisabled) ImGui::BeginDisabled();
            if (ImGui::Button("<"))
            {
                if (state.viewerAlignmentIdx > 0)
                {
                    state.viewerAlignmentIdx--;
                    applyAlignment(
                        state.viewerLeft,
                        state.viewerRight,
                        state
                            .viewerAlignments[state.viewerAlignmentIdx],
                        state.viewerLocked);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button(">"))
            {
                int maxIdx = (int)state.viewerAlignments.size() - 1;
                if (state.viewerAlignmentIdx < maxIdx)
                {
                    state.viewerAlignmentIdx++;
                    applyAlignment(
                        state.viewerLeft,
                        state.viewerRight,
                        state
                            .viewerAlignments[state.viewerAlignmentIdx],
                        state.viewerLocked);
                }
            }
            if (navDisabled) ImGui::EndDisabled();

            auto& a = state.viewerAlignments[state.viewerAlignmentIdx];
            ImGui::SameLine();
            ImGui::Text(
                "[%d/%d] [%s] R:%.1f"
                " T:(%.0f,%.0f) S:%.2f",
                state.viewerAlignmentIdx + 1,
                (int)state.viewerAlignments.size(),
                a.mode == AlignMode::Manual ? "Manual" : "Auto",
                a.rotation, a.translationX, a.translationY, a.scale);

            ImGui::SameLine();
            bool delDisabled = (int)state.viewerAlignments.size() <= 1;
            if (delDisabled) ImGui::BeginDisabled();
            if (ImGui::Button("Delete"))
            {
                int idx = state.viewerAlignmentIdx;
                state.viewerAlignments.erase(
                    state.viewerAlignments.begin() + idx);
                if (idx >= (int)state.viewerAlignments.size())
                    idx = (int)state.viewerAlignments.size() - 1;
                state.viewerAlignmentIdx = idx;
                applyAlignment(state.viewerLeft,
                               state.viewerRight,
                               state.viewerAlignments[idx],
                               state.viewerLocked);
            }
            if (delDisabled) ImGui::EndDisabled();

            ImGui::SameLine();
            if (ImGui::Button("Edit"))
            {
                state.alignEditState =
                    state.viewerAlignments[state.viewerAlignmentIdx];
                state.alignEditOriginal = state.alignEditState;
                state.alignEditOpen = true;
            }
        }
    }

    static void renderImageGallery(AppState& state)
    {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImVec2 origin = ImGui::GetCursorScreenPos();

        // Reserve space for dock bar at bottom
        float dockH = ImGui::GetFrameHeightWithSpacing() +
                      ImGui::GetStyle().ItemSpacing.y;
        float galleryH = avail.y - dockH;

        ImGui::BeginChild("GalleryArea", ImVec2(avail.x, galleryH),
                          ImGuiChildFlags_None);

        // Title bar height for sizing the window
        float titleH =
            ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.y;
        ImVec2 pad = ImGui::GetStyle().WindowPadding;
        float maxW = avail.x * 0.5f;
        float maxH = galleryH * 0.5f;

        int removeIdx = -1;
        for (int i = 0; i < (int)state.images.size(); ++i)
        {
            auto& canvas = state.images[i];

            // Scale image to fit within max bounds
            float scale = std::min(maxW / (float)canvas.image->width,
                                   maxH / (float)canvas.image->height);
            scale = std::min(scale, 1.0f);
            float dispW = canvas.image->width * scale;
            float dispH = canvas.image->height * scale;

            float tbRows = ImGui::GetFrameHeightWithSpacing() * 3.0f;
            float minW = 400.0f;
            float winW = std::max(dispW + pad.x * 2, minW);
            ImGui::SetNextWindowSize(
                ImVec2(winW, dispH + pad.y * 2 + titleH + tbRows),
                ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(
                ImVec2(origin.x + 20 + i * 30, origin.y + 20 + i * 30),
                ImGuiCond_FirstUseEver);

            char winId[128];
            snprintf(winId, sizeof(winId), "%s###gallery_%d",
                     canvas.image->name.c_str(), i);

            char canvasId[64];
            snprintf(canvasId, sizeof(canvasId), "##gcanvas_%d", i);

            bool open = true;
            if (ImGui::Begin(winId, &open,
                             ImGuiWindowFlags_NoSavedSettings))
            {
                if (ImGui::IsWindowFocused(
                        ImGuiFocusedFlags_ChildWindows))
                    state.activeGalleryImage = i;

                float toolbarH =
                    ImGui::GetFrameHeightWithSpacing() * 3.0f;
                ImVec2 region = ImGui::GetContentRegionAvail();
                float canvasH = region.y - toolbarH;
                if (canvasH > 0.0f)
                {
                    ImGui::BeginChild(canvasId, ImVec2(0, canvasH),
                                      ImGuiChildFlags_None);
                    char cid[64];
                    snprintf(cid, sizeof(cid), "##gc_%d", i);
                    canvas.renderCanvas(cid);
                    ImGui::EndChild();
                }
                char tbId[64];
                snprintf(tbId, sizeof(tbId), "##gtb_%d", i);
                canvas.renderToolbar(tbId);
            }
            ImGui::End();

            if (!open) removeIdx = i;
        }

        if (removeIdx >= 0)
        {
            state.images.erase(state.images.begin() + removeIdx);
            clampViewerIndices(removeIdx, (int)state.images.size(),
                               state.viewerLeftIdx,
                               state.viewerRightIdx,
                               state.activeGalleryImage);
        }

        if (state.images.empty())
            ImGui::Text("Use Load Image to add images");

        ImGui::EndChild();

        // --- Dock bar ---
        ImGui::Separator();
        bool hasActive =
            state.activeGalleryImage >= 0 &&
            state.activeGalleryImage < (int)state.images.size();

        if (ImGui::Button("Load Image"))
            state.imageLoadBrowser.show = true;

        ImGui::SameLine();
        ImGui::BeginDisabled(!hasActive);
        if (ImGui::Button("Save PNG"))
        {
            state.imageSaveBrowser.show = true;
            state.imageSaveTarget = state.activeGalleryImage;
            state.imageSaveBrowser.dirNeedsRefresh = true;
            state.imageSaveBrowser.fileName.clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("Load JSON"))
        {
            state.annotationFileBrowser.show = true;
            state.annotationFileSave = false;
            state.annotationFileTarget = state.activeGalleryImage;
            state.annotationFileBrowser.dirNeedsRefresh = true;
            state.annotationFileBrowser.fileName.clear();
            state.annotationFileBrowser.title = "Load Annotations";
        }
        ImGui::SameLine();
        if (ImGui::Button("Save JSON"))
        {
            state.annotationFileBrowser.show = true;
            state.annotationFileSave = true;
            state.annotationFileTarget = state.activeGalleryImage;
            state.annotationFileBrowser.dirNeedsRefresh = true;
            state.annotationFileBrowser.fileName.clear();
            state.annotationFileBrowser.title = "Save Annotations";
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(state.images.empty());
        if (ImGui::Button("Images")) state.imageListDialog.show = true;
        ImGui::EndDisabled();
    }

    static void renderAbout()
    {
        static std::string aboutText;
        if (aboutText.empty())
        {
            auto data = HelloImGui::LoadAssetFileData("about.txt");
            if (data.data)
            {
                aboutText.assign((const char*)data.data, data.dataSize);
                HelloImGui::FreeAssetFileData(&data);
            }
        }

        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGui::InputTextMultiline(
            "##about", const_cast<char*>(aboutText.c_str()),
            aboutText.size() + 1, avail, ImGuiInputTextFlags_ReadOnly);
    }

    static void renderImageSaveProgressPopup(AppState& state)
    {
        if (state.imageSaveInProgress.load())
        {
            ImGui::OpenPopup("Saving Image...");
        }
        ImVec2 ds = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowSize(ImVec2(ds.x * 0.4f, ds.y * 0.2f),
                                 ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(ds.x * 0.3f, ds.y * 0.4f),
                                ImGuiCond_Always);
        if (ImGui::BeginPopupModal(
                "Saving Image...", nullptr,
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
        {
            ImGui::Text("Saving to: %s",
                        state.imageSaveProgressPath.c_str());
            ImGui::Spacing();

            // Animated spinner dots
            int dots = (int)(ImGui::GetTime() / 0.4) % 3 + 1;
            ImGui::Text("Please wait%.*s", dots, "...");

            if (state.imageSaveDone.load())
            {
                if (state.imageSaveThread.joinable())
                    state.imageSaveThread.join();
                if (state.imageSaveResult.load() != 0)
                {
                    state.imageSaveError.show = true;
                    state.imageSaveError.message =
                        "Failed to save image "
                        "to:\n" +
                        state.imageSaveProgressPath;
                }
                state.imageSaveInProgress = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    static void consumeAlignResults(AppState& state)
    {
        if (!state.alignDialog.open &&
            state.alignDialog.workerFinished &&
            !state.alignDialog.workerResults.empty())
        {
            size_t count = state.alignDialog.workerResults.size();
            for (auto& a : state.alignDialog.workerResults)
                state.viewerAlignments.push_back(std::move(a));
            state.viewerAlignmentIdx =
                (int)state.viewerAlignments.size() - 1;
            state.alignDialog.workerResults.clear();
            state.alignDialog.workerFinished = false;
            printf(
                "mainWindow: %zu alignment(s) "
                "added\n",
                count);
        }
    }

    static void renderAlignEditPopup(AppState& state)
    {
        if (state.alignEditOpen)
        {
            ImGui::OpenPopup("Edit Alignment");
            state.alignEditOpen = false;
        }
        ImVec2 eds = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowSize(ImVec2(eds.x * 0.35f, eds.y * 0.35f),
                                 ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(ImVec2(eds.x * 0.325f, eds.y * 0.325f),
                                ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal(
                "Edit Alignment", nullptr,
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
        {
            bool changed = false;
            changed |= ImGui::SliderFloat(
                "Rotation (deg)", &state.alignEditState.rotation,
                -180.0f, 180.0f, "%.1f");
            changed |= ImGui::SliderFloat(
                "Translation X", &state.alignEditState.translationX,
                -2000.0f, 2000.0f, "%.1f");
            changed |= ImGui::SliderFloat(
                "Translation Y", &state.alignEditState.translationY,
                -2000.0f, 2000.0f, "%.1f");
            changed |=
                ImGui::SliderFloat("Scale", &state.alignEditState.scale,
                                   0.1f, 10.0f, "%.2f");

            if (changed)
            {
                applyAlignment(state.viewerLeft,
                               state.viewerRight,
                               state.alignEditState,
                               state.viewerLocked);
            }

            ImGui::Spacing();
            ImGui::Separator();
            if (ImGui::Button("Save"))
            {
                state.viewerAlignments[state.viewerAlignmentIdx] =
                    state.alignEditState;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                state.viewerAlignments[state.viewerAlignmentIdx] =
                    state.alignEditOriginal;
                applyAlignment(state.viewerLeft,
                               state.viewerRight,
                               state.alignEditOriginal,
                               state.viewerLocked);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    static void renderGui(AppState& state)
    {
        state.annotationError.render();
        state.imageSaveError.render();
        state.alignmentSaveError.render();
        state.alignmentSaveBrowser.render();
        state.imageLoadBrowser.render();
        state.imageSaveBrowser.render();
        state.annotationFileBrowser.render();
        renderImageSaveProgressPopup(state);
        state.imageListDialog.render(state.images, state.viewerLeftIdx,
                                     state.viewerRightIdx,
                                     state.activeGalleryImage);

        state.alignDialog.render();
        consumeAlignResults(state);
        renderAlignEditPopup(state);

        if (ImGui::BeginTabBar("MainTabs"))
        {
            if (ImGui::BeginTabItem("Image Viewer"))
            {
                renderImageGallery(state);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Image Comparison"))
            {
                renderImageViewer(state);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Settings"))
            {
                renderFilesAndSettings(state);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("About"))
            {
                renderAbout();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }

    static void onImageSaveOk(AppState& state,
                              const std::string& fullPath)
    {
        if (state.imageSaveTarget < 0 ||
            state.imageSaveTarget >= (int)state.images.size() ||
            state.imageSaveInProgress.load())
            return;
        auto& img = state.images[state.imageSaveTarget].image;

        int w = img->width;
        int h = img->height;
        auto buffer =
            std::make_shared<std::vector<unsigned char>>(w * h * 4);
        GLuint tex = (GLuint)(intptr_t)img->textureId;
        glBindTexture(GL_TEXTURE_2D, tex);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                      buffer->data());

        state.imageSaveInProgress = true;
        state.imageSaveDone = false;
        state.imageSaveProgressPath = fullPath;
        if (state.imageSaveThread.joinable())
            state.imageSaveThread.join();
        state.imageSaveThread = std::thread(
            [&state, fullPath, buffer, w, h]()
            {
                int res = savePngToDisk(fullPath, buffer->data(), w, h);
                state.imageSaveResult = res;
                state.imageSaveDone = true;
            });
    }

    static void onAnnotationFileOk(AppState& state,
                                   const std::string& fullPath)
    {
        if (state.annotationFileTarget < 0 ||
            state.annotationFileTarget >= (int)state.images.size())
            return;
        auto& img = state.images[state.annotationFileTarget].image;
        if (state.annotationFileSave)
        {
            if (saveAnnotationsToFile(fullPath, img->annotations) != 0)
            {
                state.annotationError.show = true;
                state.annotationError.message =
                    "Failed to save "
                    "annotations to:\n" +
                    fullPath;
            }
        }
        else
        {
            if (loadAnnotationsFromFile(fullPath, img->annotations) !=
                0)
            {
                state.annotationError.show = true;
                state.annotationError.message =
                    "Failed to load "
                    "annotations from:\n" +
                    fullPath;
            }
        }
    }

    static void onAlignmentSaveOk(AppState& state,
                                  const std::string& fullPath)
    {
        auto& a = state.viewerAlignments[state.viewerAlignmentIdx];
        jt::Json obj;
        obj.setObject();
        obj["mode"] = jt::Json(
            a.mode == AlignMode::Manual ? "Manual" : "Automatic");
        obj["rotation"] = jt::Json((double)a.rotation);
        obj["translationX"] = jt::Json((double)a.translationX);
        obj["translationY"] = jt::Json((double)a.translationY);
        obj["scale"] = jt::Json((double)a.scale);
        obj["info"] = a.info;

        std::ofstream ofs(fullPath);
        if (!ofs)
        {
            state.alignmentSaveError.show = true;
            state.alignmentSaveError.message =
                "Failed to save alignment "
                "to:\n" +
                fullPath;
            return;
        }
        ofs << obj.toStringPretty();
        if (!ofs)
        {
            state.alignmentSaveError.show = true;
            state.alignmentSaveError.message =
                "Failed to write alignment "
                "to:\n" +
                fullPath;
        }
    }

    static void onImageLoadSelect(AppState& state,
                                  const std::string& fullPath,
                                  const std::string& name)
    {
        for (auto& c : state.images)
        {
            if (c.image->path == fullPath) return;
        }
        ImageCanvas canvas;
        canvas.image->name = name;
        canvas.image->path = fullPath;
        if (!loadPngFromDisk(fullPath, canvas.image->textureId,
                             canvas.image->width, canvas.image->height))
            return;

        canvas.image->annotations.setObject();
        canvas.image->annotations["bounds"].setArray();
        canvas.image->annotations["points"].setArray();

        if (state.imageLoadBrowser.loadCorrespondingJson)
        {
            fs::path jsonPath =
                fs::path(fullPath).replace_extension(".json");
            if (fs::exists(jsonPath))
            {
                if (loadAnnotationsFromFile(
                        jsonPath.string(), canvas.image->annotations) !=
                    0)
                {
                    canvas.image->annotations.setObject();
                    canvas.image->annotations["bounds"].setArray();
                    canvas.image->annotations["points"].setArray();
                    state.annotationError.show = true;
                    state.annotationError.message =
                        "Failed to load "
                        "annotations "
                        "from:\n" +
                        jsonPath.string();
                }
            }
        }

        state.images.push_back(std::move(canvas));
    }

    static void onBeforeExit(AppState& state)
    {
        if (state.imageSaveThread.joinable())
            state.imageSaveThread.join();
        state.alignDialog.cancelWorker();
        state.alignDialog.cleanup();
        state.alignDialog.leftImage.reset();
        state.alignDialog.rightImage.reset();
        state.viewerLeft.image.reset();
        state.viewerRight.image.reset();
        state.images.clear();
    }

    void submain(void)
    {
        runSplash(1.0);

        AppState state;

        state.imageSaveError.title = "Image Save Error";
        state.annotationError.title = "Annotation Error";

        state.imageSaveBrowser.extension = ".png";
        state.imageSaveBrowser.title = "Save Image";
        state.imageSaveBrowser.onOk = [&state](const std::string& p)
        { onImageSaveOk(state, p); };

        state.annotationFileBrowser.extension = ".json";
        state.annotationFileBrowser.title = "Annotation File";
        state.annotationFileBrowser.onOk =
            [&state](const std::string& p)
        { onAnnotationFileOk(state, p); };

        state.alignmentSaveBrowser.extension = ".json";
        state.alignmentSaveBrowser.title = "Save Alignment";
        state.alignmentSaveError.title = "Alignment Save Error";
        state.alignmentSaveBrowser.onOk = [&state](const std::string& p)
        { onAlignmentSaveOk(state, p); };

        state.imageLoadBrowser.extension = ".png";
        state.imageLoadBrowser.title = "Load Image";
        state.imageLoadBrowser.onSelect =
            [&state](const std::string& p, const std::string& n)
        { onImageLoadSelect(state, p, n); };

        HelloImGui::RunnerParams params;
        params.appWindowParams.windowTitle = "ShoeComp";
        params.appWindowParams.windowGeometry.fullScreenMode =
            HelloImGui::FullScreenMode::FullMonitorWorkArea;
        params.imGuiWindowParams.defaultImGuiWindowType =
            HelloImGui::DefaultImGuiWindowType::ProvideFullScreenWindow;
        params.callbacks.PostInit = []()
        { ImGui::GetIO().FontGlobalScale = 2.5f; };
        params.callbacks.ShowGui = [&state]() { renderGui(state); };
        params.callbacks.BeforeExit = [&state]()
        { onBeforeExit(state); };

        HelloImGui::Run(params);
    }

}  // namespace shoecomp
