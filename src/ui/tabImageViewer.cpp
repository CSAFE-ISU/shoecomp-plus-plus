#include "ui/mainWindow.h"
#include "ui/imageCanvas.h"
#include "ui/uiHelpers.h"
#include "formats/annotationIo.h"
#include <algorithm>
#include <cmath>

#ifdef __EMSCRIPTEN__
extern "C"
{
    void scpp_emOpenImagePicker();
    void scpp_emOpenJsonPicker();
    void scpp_emDownloadString(const char* data, int len,
                               const char* filename, int filenameLen);
}
#endif

namespace shoecomp
{
    void renderImageGallery(AppState& state)
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
            bool isActive = (state.activeGalleryImage == i);
            ImGui::SetNextWindowBgAlpha(1.0f);
            ImGui::SetNextWindowSizeConstraints(
                ImVec2(200.0f, titleH + 10.0f),
                ImVec2(avail.x, galleryH));
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
                                          origin.x + avail.x - ws.x),
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
                state.viewerLeft = ImageCanvas{};
                state.viewerRight = ImageCanvas{};
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

        // --- Dock bar ---
        ImGui::Separator();
        bool hasActive =
            state.activeGalleryImage >= 0 &&
            state.activeGalleryImage < (int)state.images.size();

        if (ImGui::Button("Load Image"))
        {
#ifdef __EMSCRIPTEN__
            scpp_emOpenImagePicker();
#else
            state.imageLoadBrowser.show = true;
#endif
        }

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
#ifdef __EMSCRIPTEN__
            state.annotationFileSave = false;
            state.annotationFileTarget = state.activeGalleryImage;
            scpp_emOpenJsonPicker();
#else
            state.annotationFileBrowser.show = true;
            state.annotationFileSave = false;
            state.annotationFileTarget = state.activeGalleryImage;
            state.annotationFileBrowser.dirNeedsRefresh = true;
            state.annotationFileBrowser.fileName.clear();
            state.annotationFileBrowser.contextLabel.clear();
            state.annotationFileBrowser.extensionChoices = {".json",
                                                            ""};
            state.annotationFileBrowser.title = "Load Annotations";
#endif
        }
        ImGui::SameLine();
        if (ImGui::Button("Save JSON"))
        {
#ifdef __EMSCRIPTEN__
            auto& img = state.images[state.activeGalleryImage].image;
            jt::Json copy = img->annotations;
            std::string data = copy.toStringPretty();
            if (data.empty())
            {
                state.annotationError.show = true;
                state.annotationError.message =
                    "Failed to serialize annotations.";
            }
            else
            {
                std::string fname =
                    img->name.empty() ? std::string("annotations.json")
                                      : img->name + ".json";
                scpp_emDownloadString(data.c_str(), (int)data.size(),
                                      fname.c_str(), (int)fname.size());
            }
#else
            state.annotationFileBrowser.show = true;
            state.annotationFileSave = true;
            state.annotationFileTarget = state.activeGalleryImage;
            state.annotationFileBrowser.dirNeedsRefresh = true;
            state.annotationFileBrowser.fileName.clear();
            {
                auto& canvas = state.images[state.activeGalleryImage];
                state.annotationFileBrowser.contextLabel =
                    canvas.image ? canvas.image->name : std::string();
            }
            state.annotationFileBrowser.extensionChoices.clear();
            state.annotationFileBrowser.title = "Save Annotations";
#endif
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(state.images.empty());
        if (ImGui::Button("Images")) state.imageListDialog.show = true;
        ImGui::EndDisabled();
    }

}  // namespace shoecomp
