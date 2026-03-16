#include "ui/mainWindow.h"
#include "ui/imageCanvas.h"
#include "formats/png.h"
#include "formats/annotationIo.h"
#include "json.h"
#include "hello_imgui/hello_imgui_include_opengl.h"
#include <algorithm>
#include <cmath>
#include <filesystem>

namespace shoecomp
{
    namespace fs = std::filesystem;

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
            ImGui::Text("%s",
                state.images[i].image->name.c_str());
            ImGui::PopID();
        }
        if (removeIdx >= 0)
        {
            state.images.erase(
                state.images.begin() + removeIdx);
            if (state.viewerLeftIdx >=
                (int)state.images.size())
                state.viewerLeftIdx =
                    (int)state.images.size() - 1;
            if (state.viewerRightIdx >=
                (int)state.images.size())
                state.viewerRightIdx =
                    (int)state.images.size() - 1;
        }
    }

    static void renderFilesAndSettings(AppState& state)
    {
        ImGui::BeginChild("SettingsPane", ImVec2(0, 0),
                          ImGuiChildFlags_Borders);
        renderSettings(state);
        ImGui::EndChild();
    }

    static void renderLockToggle(bool& locked)
    {
        if (ImGui::Button(locked ? "Unlock" : "Lock")) locked = !locked;
    }

    static void renderSingleViewer(
        AppState& state,
        int& selectedIdx,
        int otherIdx,
        ImageCanvas& viewer,
        const char* label,
        ImageViewState* linked = nullptr)
    {
        const char* preview =
            (selectedIdx >= 0 &&
             selectedIdx < (int)state.images.size())
                ? state.images[selectedIdx]
                      .image->name.c_str()
                : "<none>";

        if (ImGui::BeginCombo(label, preview))
        {
            if (ImGui::Selectable("<none>",
                                  selectedIdx < 0))
            {
                selectedIdx = -1;
                viewer.viewState.zoom =
                    viewer.viewState.zoomTarget =
                        1.0f;
                viewer.viewState.pan =
                    viewer.viewState.panTarget =
                        ImVec2(0, 0);
            }
            for (int i = 0;
                 i < (int)state.images.size(); ++i)
            {
                if (i == otherIdx) continue;
                bool selected = (i == selectedIdx);
                if (ImGui::Selectable(
                        state.images[i]
                            .image->name.c_str(),
                        selected))
                {
                    selectedIdx = i;
                    viewer.viewState.zoom =
                        viewer.viewState.zoomTarget =
                            0.0f;
                    viewer.viewState.pan =
                        viewer.viewState.panTarget =
                            ImVec2(0, 0);
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (selectedIdx < 0 ||
            selectedIdx >= (int)state.images.size())
            return;

        viewer.image =
            state.images[selectedIdx].image;
        float toolbarH =
            ImGui::GetFrameHeightWithSpacing() *
            3.0f;
        ImVec2 region =
            ImGui::GetContentRegionAvail();
        float canvasH = region.y - toolbarH;
        if (canvasH > 0.0f)
        {
            ImGui::BeginChild("##cvs",
                              ImVec2(0, canvasH),
                              ImGuiChildFlags_None);
            viewer.renderCanvas("##canvas",
                                linked);
            ImGui::EndChild();
        }
        viewer.renderToolbar(label, linked);
    }

    static void renderImageViewer(AppState& state)
    {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float dockH =
            ImGui::GetFrameHeightWithSpacing() +
            ImGui::GetStyle().ItemSpacing.y;
        float contentH = avail.y - dockH;

        ImGui::BeginChild("ComparisonContent",
                          ImVec2(avail.x, contentH),
                          ImGuiChildFlags_None);
        {
            float totalW =
                ImGui::GetContentRegionAvail().x;
            float splitterW = 8.0f;
            float leftW =
                totalW * state.viewerSplitRatio -
                splitterW * 0.5f;
            float rightW =
                totalW *
                    (1.0f -
                     state.viewerSplitRatio) -
                splitterW * 0.5f;

            ImGui::BeginChild(
                "LeftViewer", ImVec2(leftW, 0),
                ImGuiChildFlags_Borders);
            renderSingleViewer(
                state, state.viewerLeftIdx,
                state.viewerRightIdx,
                state.viewerLeft, "##Left",
                state.viewerLocked
                    ? &state.viewerRight.viewState
                    : nullptr);
            ImGui::EndChild();

            ImGui::SameLine();

            // Draggable splitter
            float height =
                ImGui::GetContentRegionAvail().y;
            ImGui::Button("##Splitter",
                          ImVec2(splitterW, height));
            if (ImGui::IsItemActive())
            {
                float delta =
                    ImGui::GetIO().MouseDelta.x;
                state.viewerSplitRatio +=
                    delta / totalW;
                state.viewerSplitRatio = std::clamp(
                    state.viewerSplitRatio, 0.1f,
                    0.9f);
            }
            if (ImGui::IsItemHovered() ||
                ImGui::IsItemActive())
                ImGui::SetMouseCursor(
                    ImGuiMouseCursor_ResizeEW);

            ImGui::SameLine();

            ImGui::BeginChild(
                "RightViewer", ImVec2(rightW, 0),
                ImGuiChildFlags_Borders);
            renderSingleViewer(
                state, state.viewerRightIdx,
                state.viewerLeftIdx,
                state.viewerRight, "##Right",
                state.viewerLocked
                    ? &state.viewerLeft.viewState
                    : nullptr);
            ImGui::EndChild();
        }
        ImGui::EndChild();

        // --- Dock bar ---
        ImGui::Separator();
        if (ImGui::Button(state.viewerLocked
                              ? "Unlock"
                              : "Lock"))
            state.viewerLocked = !state.viewerLocked;
    }

    static void renderImageGallery(AppState& state)
    {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImVec2 origin = ImGui::GetCursorScreenPos();

        // Reserve space for dock bar at bottom
        float dockH =
            ImGui::GetFrameHeightWithSpacing() +
            ImGui::GetStyle().ItemSpacing.y;
        float galleryH = avail.y - dockH;

        ImGui::BeginChild("GalleryArea",
                          ImVec2(avail.x, galleryH),
                          ImGuiChildFlags_None);

        // Title bar height for sizing the window
        float titleH =
            ImGui::GetFrameHeight() +
            ImGui::GetStyle().FramePadding.y;
        ImVec2 pad = ImGui::GetStyle().WindowPadding;
        float maxW = avail.x * 0.5f;
        float maxH = galleryH * 0.5f;

        int removeIdx = -1;
        for (int i = 0;
             i < (int)state.images.size(); ++i)
        {
            auto& canvas = state.images[i];

            // Scale image to fit within max bounds
            float scale = std::min(
                maxW / (float)canvas.image->width,
                maxH /
                    (float)canvas.image->height);
            scale = std::min(scale, 1.0f);
            float dispW =
                canvas.image->width * scale;
            float dispH =
                canvas.image->height * scale;

            float tbRows =
                ImGui::GetFrameHeightWithSpacing() *
                3.0f;
            float minW = 400.0f;
            float winW =
                std::max(dispW + pad.x * 2, minW);
            ImGui::SetNextWindowSize(
                ImVec2(winW,
                       dispH + pad.y * 2 + titleH +
                           tbRows),
                ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(
                ImVec2(origin.x + 20 + i * 30,
                       origin.y + 20 + i * 30),
                ImGuiCond_FirstUseEver);

            char winId[128];
            snprintf(winId, sizeof(winId),
                     "%s###gallery_%d",
                     canvas.image->name.c_str(), i);

            char canvasId[64];
            snprintf(canvasId, sizeof(canvasId),
                     "##gcanvas_%d", i);

            bool open = true;
            if (ImGui::Begin(
                    winId, &open,
                    ImGuiWindowFlags_NoSavedSettings))
            {
                if (ImGui::IsWindowFocused(
                        ImGuiFocusedFlags_ChildWindows))
                    state.activeGalleryImage = i;

                float toolbarH =
                    ImGui::GetFrameHeightWithSpacing() *
                    3.0f;
                ImVec2 region =
                    ImGui::GetContentRegionAvail();
                float canvasH =
                    region.y - toolbarH;
                if (canvasH > 0.0f)
                {
                    ImGui::BeginChild(
                        canvasId,
                        ImVec2(0, canvasH),
                        ImGuiChildFlags_None);
                    char cid[64];
                    snprintf(cid, sizeof(cid),
                             "##gc_%d", i);
                    canvas.renderCanvas(cid);
                    ImGui::EndChild();
                }
                char tbId[64];
                snprintf(tbId, sizeof(tbId),
                         "##gtb_%d", i);
                canvas.renderToolbar(tbId);
            }
            ImGui::End();

            if (!open) removeIdx = i;
        }

        if (removeIdx >= 0)
        {
            state.images.erase(
                state.images.begin() + removeIdx);
            if (state.viewerLeftIdx >=
                (int)state.images.size())
                state.viewerLeftIdx =
                    (int)state.images.size() - 1;
            if (state.viewerRightIdx >=
                (int)state.images.size())
                state.viewerRightIdx =
                    (int)state.images.size() - 1;
            if (state.activeGalleryImage ==
                removeIdx)
                state.activeGalleryImage = -1;
            else if (state.activeGalleryImage >
                     removeIdx)
                state.activeGalleryImage--;
        }

        if (state.images.empty())
            ImGui::Text(
                "Use Load Image to add images");

        ImGui::EndChild();

        // --- Dock bar ---
        ImGui::Separator();
        bool hasActive =
            state.activeGalleryImage >= 0 &&
            state.activeGalleryImage <
                (int)state.images.size();

        if (ImGui::Button("Load Image"))
            state.imageLoadBrowser.show = true;

        ImGui::SameLine();
        ImGui::BeginDisabled(!hasActive);
        if (ImGui::Button("Save PNG"))
        {
            state.imageSaveBrowser.show = true;
            state.imageSaveTarget =
                state.activeGalleryImage;
            state.imageSaveBrowser.dirNeedsRefresh =
                true;
            state.imageSaveBrowser.fileName.clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("Load JSON"))
        {
            state.annotationFileBrowser.show = true;
            state.annotationFileSave = false;
            state.annotationFileTarget =
                state.activeGalleryImage;
            state.annotationFileBrowser
                .dirNeedsRefresh = true;
            state.annotationFileBrowser.fileName
                .clear();
            state.annotationFileBrowser.title =
                "Load Annotations";
        }
        ImGui::SameLine();
        if (ImGui::Button("Save JSON"))
        {
            state.annotationFileBrowser.show = true;
            state.annotationFileSave = true;
            state.annotationFileTarget =
                state.activeGalleryImage;
            state.annotationFileBrowser
                .dirNeedsRefresh = true;
            state.annotationFileBrowser.fileName
                .clear();
            state.annotationFileBrowser.title =
                "Save Annotations";
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(state.images.empty());
        if (ImGui::Button("Images"))
            state.imageListDialog.show = true;
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

    static void renderImageSaveProgressPopup(
        AppState& state)
    {
        if (state.imageSaveInProgress.load())
        {
            ImGui::OpenPopup("Saving Image...");
        }
        ImVec2 ds = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowSize(
            ImVec2(ds.x * 0.4f, ds.y * 0.2f),
            ImGuiCond_Always);
        ImGui::SetNextWindowPos(
            ImVec2(ds.x * 0.3f, ds.y * 0.4f),
            ImGuiCond_Always);
        if (ImGui::BeginPopupModal(
                "Saving Image...", nullptr,
                ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove))
        {
            ImGui::Text("Saving to: %s",
                        state.imageSaveProgressPath
                            .c_str());
            ImGui::Spacing();

            // Animated spinner dots
            int dots =
                (int)(ImGui::GetTime() / 0.4) % 3 +
                1;
            ImGui::Text("Please wait%.*s", dots,
                        "...");

            if (state.imageSaveDone.load())
            {
                if (state.imageSaveThread.joinable())
                    state.imageSaveThread.join();
                if (state.imageSaveResult.load() !=
                    0)
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

    static void renderGui(AppState& state)
    {
        state.annotationError.render();
        state.imageSaveError.render();
        state.imageLoadBrowser.render();
        state.imageSaveBrowser.render();
        state.annotationFileBrowser.render();
        renderImageSaveProgressPopup(state);
        state.imageListDialog.render(
            state.images, state.viewerLeftIdx,
            state.viewerRightIdx,
            state.activeGalleryImage);

        if (ImGui::BeginTabBar("MainTabs"))
        {
            if (ImGui::BeginTabItem("Image Viewer"))
            {
                renderImageGallery(state);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(
                    "Image Comparison"))
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

    void submain(void)
    {
        runSplash(1.0);

        AppState state;

        // Configure error popup titles
        state.imageSaveError.title =
            "Image Save Error";
        state.annotationError.title =
            "Annotation Error";

        // Configure image save browser
        state.imageSaveBrowser.extension = ".png";
        state.imageSaveBrowser.title = "Save Image";
        state.imageSaveBrowser.onOk =
            [&state](const std::string& fullPath)
        {
            if (state.imageSaveTarget >= 0 &&
                state.imageSaveTarget <
                    (int)state.images.size() &&
                !state.imageSaveInProgress.load())
            {
                auto& img =
                    state.images[state.imageSaveTarget]
                        .image;

                // GL readback on UI thread
                int w = img->width;
                int h = img->height;
                auto buffer =
                    std::make_shared<
                        std::vector<unsigned char>>(
                        w * h * 4);
                GLuint tex = (GLuint)(intptr_t)
                    img->textureId;
                glBindTexture(GL_TEXTURE_2D, tex);
                glGetTexImage(GL_TEXTURE_2D, 0,
                              GL_RGBA,
                              GL_UNSIGNED_BYTE,
                              buffer->data());

                // Launch background thread
                state.imageSaveInProgress = true;
                state.imageSaveDone = false;
                state.imageSaveProgressPath =
                    fullPath;
                if (state.imageSaveThread.joinable())
                    state.imageSaveThread.join();
                state.imageSaveThread = std::thread(
                    [&state, fullPath, buffer, w,
                     h]()
                    {
                        int res = savePngToDisk(
                            fullPath,
                            buffer->data(), w, h);
                        state.imageSaveResult = res;
                        state.imageSaveDone = true;
                    });
            }
        };

        // Configure annotation file browser
        state.annotationFileBrowser.extension =
            ".json";
        state.annotationFileBrowser.title =
            "Annotation File";
        state.annotationFileBrowser.onOk =
            [&state](const std::string& fullPath)
        {
            if (state.annotationFileTarget >= 0 &&
                state.annotationFileTarget <
                    (int)state.images.size())
            {
                auto& img =
                    state
                        .images
                            [state
                                 .annotationFileTarget]
                        .image;
                if (state.annotationFileSave)
                {
                    if (saveAnnotationsToFile(
                            fullPath,
                            img->annotations) != 0)
                    {
                        state.annotationError.show =
                            true;
                        state.annotationError
                            .message =
                            "Failed to save "
                            "annotations to:\n" +
                            fullPath;
                    }
                }
                else
                {
                    if (loadAnnotationsFromFile(
                            fullPath,
                            img->annotations) != 0)
                    {
                        state.annotationError.show =
                            true;
                        state.annotationError
                            .message =
                            "Failed to load "
                            "annotations from:\n" +
                            fullPath;
                    }
                }
            }
        };

        // Configure image load browser
        state.imageLoadBrowser.extension = ".png";
        state.imageLoadBrowser.title = "Load Image";
        state.imageLoadBrowser.onSelect =
            [&state](const std::string& fullPath,
                     const std::string& name)
        {
            bool alreadyLoaded = false;
            for (auto& c : state.images)
            {
                if (c.image->path == fullPath)
                {
                    alreadyLoaded = true;
                    break;
                }
            }
            if (!alreadyLoaded)
            {
                ImageCanvas canvas;
                canvas.image->name = name;
                canvas.image->path = fullPath;
                if (loadPngFromDisk(
                        fullPath,
                        canvas.image->textureId,
                        canvas.image->width,
                        canvas.image->height))
                {
                    canvas.image->annotations
                        .setObject();
                    canvas.image
                        ->annotations["bounds"]
                        .setArray();
                    canvas.image
                        ->annotations["points"]
                        .setArray();
                    state.images.push_back(
                        std::move(canvas));
                }
            }
        };

        HelloImGui::RunnerParams params;
        params.appWindowParams.windowTitle =
            "ShoeComp";
        params.appWindowParams.windowGeometry
            .fullScreenMode = HelloImGui::
                FullScreenMode::FullMonitorWorkArea;
        params.imGuiWindowParams
            .defaultImGuiWindowType =
            HelloImGui::DefaultImGuiWindowType::
                ProvideFullScreenWindow;
        params.callbacks.PostInit = []()
        { ImGui::GetIO().FontGlobalScale = 2.5f; };
        params.callbacks.ShowGui = [&state]()
        { renderGui(state); };
        params.callbacks.BeforeExit = [&state]()
        {
            if (state.imageSaveThread.joinable())
                state.imageSaveThread.join();
            state.viewerLeft.image.reset();
            state.viewerRight.image.reset();
            state.images.clear();
        };

        HelloImGui::Run(params);
    }

}  // namespace shoecomp
