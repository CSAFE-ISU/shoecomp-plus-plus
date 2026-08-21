#include "ui/mainWindow.h"
#include "ui/imageCanvas2d.h"
#include "ui/loadBrowser.h"
#include "ui/saveBrowser.h"
#include "ui/uiHelpers.h"
#include "calc/onnxRuntime.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace shoecomp
{
    void renderImageGallery(AppState& state)
    {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float contentW = avail.x;

        ImGui::BeginChild("GalleryArea", ImVec2(contentW, avail.y),
                          ImGuiChildFlags_None);
        ImVec2 origin = ImGui::GetCursorScreenPos();
        float galleryH = ImGui::GetContentRegionAvail().y;

        // Title bar height for sizing the window
        float titleH =
            ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.y;
        ImVec2 pad = ImGui::GetStyle().WindowPadding;
        float maxW = contentW * 0.5f;
        float maxH = galleryH * 0.5f;

        int removeIdx = -1;
        for (int i = 0; i < (int)state.images.size(); ++i)
        {
            ImageCanvas& canvas = *state.images[i];

            // Scale image to fit within max bounds
            float scale = std::min(maxW / (float)canvas.width(),
                                   maxH / (float)canvas.height());
            scale = std::min(scale, 1.0f);
            float dispW = canvas.width() * scale;
            float dispH = canvas.height() * scale;

            // One toolbar row (view controls only) plus padding.
            float tbRows = ImGui::GetFrameHeightWithSpacing() * 1.6f;
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
                     canvas.name().c_str(), i);

            char canvasId[64];
            snprintf(canvasId, sizeof(canvasId), "##gcanvas_%d", i);

            bool open = true;
            bool isActive = (state.activeGalleryImage == i);
            ImGui::SetNextWindowBgAlpha(1.0f);
            ImGui::SetNextWindowSizeConstraints(
                ImVec2(200.0f, titleH + 10.0f),
                ImVec2(contentW, galleryH));
            if (canvas.minimized != canvas.lastMinimized)
                ImGui::SetNextWindowCollapsed(canvas.minimized);
            int styleColorsPushed = 0;
            if (isActive)
            {
                ImVec4 c = state.settings.activeImageColor;
                ImGui::PushStyleColor(ImGuiCol_TitleBgActive, c);
                ImGui::PushStyleColor(ImGuiCol_TitleBg, c);
                ImGui::PushStyleColor(ImGuiCol_Border, c);
                styleColorsPushed = 3;
                ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,
                                    3.0f);
            }
            bool beginOpen = ImGui::Begin(
                winId, &open, ImGuiWindowFlags_NoSavedSettings);
            canvas.minimized = ImGui::IsWindowCollapsed();
            canvas.lastMinimized = canvas.minimized;
            {
                ImVec2 wp = ImGui::GetWindowPos();
                ImVec2 ws = ImGui::GetWindowSize();
                ImVec2 clamped(std::clamp(wp.x, origin.x,
                                          origin.x + contentW - ws.x),
                               std::clamp(wp.y, origin.y,
                                          origin.y + galleryH - ws.y));
                if (clamped.x != wp.x || clamped.y != wp.y)
                    ImGui::SetWindowPos(clamped);
            }
            if (beginOpen)
            {
                if (ImGui::IsWindowFocused(
                        ImGuiFocusedFlags_ChildWindows))
                    state.activeGalleryImage = i;

                float toolbarH =
                    ImGui::GetFrameHeightWithSpacing() * 1.6f * 0.85f;
                float trayH = ImGui::GetFrameHeightWithSpacing();
                ImVec2 region = ImGui::GetContentRegionAvail();
                float canvasH = region.y - toolbarH - trayH;
                if (canvasH > 0.0f)
                {
                    ImGui::BeginChild(canvasId, ImVec2(0, canvasH),
                                      ImGuiChildFlags_None);
                    char cid[64];
                    snprintf(cid, sizeof(cid), "##gc_%d", i);
                    canvas.renderCanvas(cid);
                    ImGui::EndChild();
                }
                if (ImageCanvas2D* c2d = asCanvas2D(canvas))
                    c2d->renderMarkupTray();
                char tbId[64];
                snprintf(tbId, sizeof(tbId), "##gtb_%d", i);
                canvas.renderToolbar(tbId);
            }
            ImGui::End();
            if (styleColorsPushed)
            {
                ImGui::PopStyleColor(styleColorsPushed);
                ImGui::PopStyleVar();
            }

            if (!open) removeIdx = i;
        }

        if (removeIdx >= 0)
        {
            if (removeIdx == state.viewerLeftIdx ||
                removeIdx == state.viewerRightIdx)
            {
                state.viewerLocked = false;
                state.viewerAlignments = {AlignState{}};
                state.viewerAlignmentIdx = 0;
                state.alignEditOpen = false;
                state.alignEditPopupVisible = false;
                state.viewerLeftIdx = -1;
                state.viewerRightIdx = -1;
                state.viewerLeft = std::make_unique<ImageCanvas2D>();
                state.viewerRight = std::make_unique<ImageCanvas2D>();
            }
            state.images.erase(state.images.begin() + removeIdx);
            clampViewerIndices(removeIdx, (int)state.images.size(),
                               state.viewerLeftIdx,
                               state.viewerRightIdx,
                               state.activeGalleryImage);
        }

        if (state.images.empty())
        {
            // Centered, faded message when no images are loaded
            ImVec2 regionAvail = ImGui::GetContentRegionAvail();
            const char* msg = "Load images to begin";

            // Calculate font size to occupy 60% of available width
            float targetWidth = regionAvail.x * 0.6f;
            ImVec2 baseSize = ImGui::CalcTextSize(msg);
            float scale = targetWidth / baseSize.x;

            ImGui::SetWindowFontScale(scale);
            ImVec2 textSize = ImGui::CalcTextSize(msg);
            ImGui::SetWindowFontScale(1.0f);

            // Center the text
            float posX = (regionAvail.x - textSize.x) * 0.5f;
            float posY = (regionAvail.y - textSize.y) * 0.5f;

            ImGui::SetCursorPos(ImVec2(posX, posY));
            ImGui::PushStyleColor(
                ImGuiCol_Text,
                ImVec4(0.5f, 0.5f, 0.5f, 0.4f));  // Faded gray
            ImGui::SetWindowFontScale(scale);
            ImGui::TextUnformatted(msg);
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();
        }

        ImGui::EndChild();
    }

}  // namespace shoecomp
