#include "ui.h"
#include "formats.h"
#include "hello_imgui/hello_imgui_include_opengl.h"
#include <algorithm>
#include <filesystem>

namespace shoecomp
{
    namespace fs = std::filesystem;

    static void runSplash(double duration)
    {
        double startTime = 0.0;

        HelloImGui::RunnerParams params;
        params.appWindowParams.windowTitle = "ShoeComp";
        params.appWindowParams.windowGeometry.size = {
            640, 360};
        params.appWindowParams.borderless = true;
        params.appWindowParams.windowGeometry
            .positionMode = HelloImGui::
                WindowPositionMode::MonitorCenter;
        params.imGuiWindowParams
            .defaultImGuiWindowType = HelloImGui::
                DefaultImGuiWindowType::
                    ProvideFullScreenWindow;
        params.callbacks.PostInit = []()
        { ImGui::GetIO().FontGlobalScale = 2.5f; };
        params.callbacks.ShowGui =
            [&startTime, duration]()
        {
            if (startTime == 0.0)
                startTime = ImGui::GetTime();

            double elapsed =
                ImGui::GetTime() - startTime;

            ImVec2 winSize = ImGui::GetWindowSize();

            const char* title = "ShoeComp";
            ImVec2 titleSize =
                ImGui::CalcTextSize(title);
            ImGui::SetCursorPos(ImVec2(
                (winSize.x - titleSize.x) * 0.5f,
                (winSize.y - titleSize.y) * 0.5f
                    - 30.0f));
            ImGui::Text("%s", title);

            // Animated dots: cycle 1-3
            int dots =
                (int)(elapsed / 0.4) % 3 + 1;
            char subtitle[16];
            snprintf(subtitle, sizeof(subtitle),
                     "Loading%.*s", dots, "...");
            // Use fixed width so text doesn't shift
            const char* widest = "Loading...";
            ImVec2 wSize =
                ImGui::CalcTextSize(widest);
            ImGui::SetCursorPos(ImVec2(
                (winSize.x - wSize.x) * 0.5f,
                (winSize.y - wSize.y) * 0.5f
                    + 30.0f));
            ImGui::Text("%s", subtitle);

            if (elapsed >= duration)
                HelloImGui::GetRunnerParams()
                    ->appShallExit = true;
        };

        HelloImGui::Run(params);
    }

    static void renderFileBrowser(AppState& state)
    {
        ImGui::Text("Directory: %s",
                     state.currentDir.c_str());
        ImGui::SameLine();
        if (ImGui::Button("Refresh"))
            state.dirNeedsRefresh = true;

        if (state.dirNeedsRefresh)
        {
            state.dirEntries.clear();
            state.dirEntries.push_back("..");
            try
            {
                for (auto& entry :
                     fs::directory_iterator(state.currentDir))
                {
                    std::string name =
                        entry.path().filename().string();
                    if (entry.is_directory())
                        state.dirEntries.push_back(
                            name + "/");
                    else if (entry.path().extension()
                             == ".png")
                        state.dirEntries.push_back(name);
                }
            }
            catch (...)
            {
            }
            std::sort(state.dirEntries.begin() + 1,
                      state.dirEntries.end());
            state.dirNeedsRefresh = false;
        }

        ImGui::BeginChild("FileList",
                          ImVec2(0, 0),
                          ImGuiChildFlags_None);
        for (auto& entry : state.dirEntries)
        {
            bool isDir = entry == ".."
                || entry.back() == '/';
            ImGuiSelectableFlags flags = isDir
                ? ImGuiSelectableFlags_None
                : ImGuiSelectableFlags_AllowDoubleClick;

            if (ImGui::Selectable(entry.c_str(), false,
                                  flags))
            {
                if (entry == "..")
                {
                    try
                    {
                        state.currentDir =
                            fs::canonical(
                                fs::path(state.currentDir)
                                / "..")
                                .string();
                    }
                    catch (...)
                    {
                    }
                    state.dirNeedsRefresh = true;
                }
                else if (entry.back() == '/')
                {
                    std::string dirName =
                        entry.substr(
                            0, entry.size() - 1);
                    try
                    {
                        state.currentDir =
                            fs::canonical(
                                fs::path(
                                    state.currentDir)
                                / dirName)
                                .string();
                    }
                    catch (...)
                    {
                    }
                    state.dirNeedsRefresh = true;
                }
                else if (ImGui::IsMouseDoubleClicked(
                             ImGuiMouseButton_Left))
                {
                    std::string fullPath =
                        fs::canonical(
                            fs::path(state.currentDir)
                            / entry)
                            .string();
                    bool alreadyLoaded = false;
                    for (auto& img : state.images)
                    {
                        if (img.path == fullPath)
                        {
                            alreadyLoaded = true;
                            break;
                        }
                    }
                    if (!alreadyLoaded)
                    {
                        LoadedImage img;
                        img.name = entry;
                        img.path = fullPath;
                        if (loadPngFromDisk(
                                fullPath,
                                img.textureId,
                                img.width,
                                img.height))
                        {
                            state.images.push_back(
                                img);
                        }
                    }
                }
            }
        }
        ImGui::EndChild();
    }

    static void renderSettings(AppState& state)
    {
        ImGui::Text("Loaded images:");
        int removeIdx = -1;
        for (int i = 0;
             i < (int)state.images.size();
             ++i)
        {
            ImGui::PushID(i);
            if (ImGui::Button("X"))
                removeIdx = i;
            ImGui::SameLine();
            ImGui::Text("%s",
                        state.images[i].name.c_str());
            ImGui::PopID();
        }
        if (removeIdx >= 0)
        {
            freeTexture(
                state.images[removeIdx].textureId);
            state.images.erase(
                state.images.begin() + removeIdx);
            if (state.viewerLeftIdx >= (int)state.images.size())
                state.viewerLeftIdx =
                    (int)state.images.size() - 1;
            if (state.viewerRightIdx >= (int)state.images.size())
                state.viewerRightIdx =
                    (int)state.images.size() - 1;
        }
    }

    static void renderFilesAndSettings(AppState& state)
    {
        float totalH = ImGui::GetContentRegionAvail().y;
        float halfH = totalH * 0.5f;

        ImGui::BeginChild("FileBrowserPane",
                          ImVec2(0, halfH),
                          ImGuiChildFlags_Borders);
        renderFileBrowser(state);
        ImGui::EndChild();

        ImGui::BeginChild("SettingsPane",
                          ImVec2(0, 0),
                          ImGuiChildFlags_Borders);
        renderSettings(state);
        ImGui::EndChild();
    }

    static void renderSingleViewer(AppState& state,
                                   int& selectedIdx,
                                   int otherIdx,
                                   float& zoom,
                                   ImVec2& pan,
                                   const char* label)
    {
        const char* preview = (selectedIdx >= 0
                               && selectedIdx
                                      < (int)state.images
                                            .size())
            ? state.images[selectedIdx].name.c_str()
            : "<none>";

        if (ImGui::BeginCombo(label, preview))
        {
            // None option
            if (ImGui::Selectable("<none>",
                                  selectedIdx < 0))
            {
                selectedIdx = -1;
                zoom = 1.0f;
                pan = ImVec2(0, 0);
            }
            for (int i = 0;
                 i < (int)state.images.size();
                 ++i)
            {
                if (i == otherIdx)
                    continue;
                bool selected = (i == selectedIdx);
                if (ImGui::Selectable(
                        state.images[i].name.c_str(),
                        selected))
                {
                    selectedIdx = i;
                    zoom = 0.0f;
                    pan = ImVec2(0, 0);
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (selectedIdx < 0
            || selectedIdx >= (int)state.images.size())
            return;

        auto& img = state.images[selectedIdx];
        ImVec2 avail = ImGui::GetContentRegionAvail();

        // Fit image to available space, then apply zoom
        float scaleX = avail.x / (float)img.width;
        float scaleY = avail.y / (float)img.height;
        float baseScale = std::min(scaleX, scaleY);
        // Initial zoom: fit width to pane
        if (zoom <= 0.0f)
            zoom = scaleX / baseScale;
        float dispW = img.width * baseScale * zoom;
        float dispH = img.height * baseScale * zoom;

        // Canvas for clipping and input
        ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton(
            "##canvas", avail,
            ImGuiButtonFlags_MouseButtonLeft);
        bool hovered = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();
        ImGuiIO& io = ImGui::GetIO();

        // Ctrl + scroll to zoom
        if (hovered && io.KeyCtrl
            && io.MouseWheel != 0.0f)
        {
            float oldZoom = zoom;
            zoom *= (io.MouseWheel > 0) ? 1.15f : 0.87f;
            zoom = std::clamp(zoom, 0.1f, 50.0f);
            // Zoom toward mouse position
            ImVec2 mouse = ImVec2(
                io.MousePos.x - canvasPos.x,
                io.MousePos.y - canvasPos.y);
            float ratio = zoom / oldZoom;
            pan.x = (1.0f - ratio)
                    * (mouse.x - avail.x * 0.5f)
                + ratio * pan.x;
            pan.y = (1.0f - ratio)
                    * (mouse.y - avail.y * 0.5f)
                + ratio * pan.y;
        }

        // Normal scroll to pan vertically
        if (hovered && !io.KeyCtrl
            && io.MouseWheel != 0.0f)
            pan.y += io.MouseWheel * 30.0f;

        // Ctrl + drag to pan
        if (active && io.KeyCtrl
            && ImGui::IsMouseDragging(
                ImGuiMouseButton_Left))
        {
            pan.x += io.MouseDelta.x;
            pan.y += io.MouseDelta.y;
        }

        // Clamp pan: allow scrolling to image edges
        float limX = std::max(avail.x, dispW) * 0.5f;
        float limY = std::max(avail.y, dispH) * 0.5f;
        pan.x = std::clamp(pan.x, -limX, limX);
        pan.y = std::clamp(pan.y, -limY, limY);

        // Draw image at pan offset, clipped to canvas
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->PushClipRect(
            canvasPos,
            ImVec2(canvasPos.x + avail.x,
                   canvasPos.y + avail.y),
            true);
        // Center image then apply pan
        float ox =
            canvasPos.x + (avail.x - dispW) * 0.5f
            + pan.x;
        float oy =
            canvasPos.y + (avail.y - dispH) * 0.5f
            + pan.y;
        dl->AddImage(img.textureId,
                     ImVec2(ox, oy),
                     ImVec2(ox + dispW, oy + dispH));
        dl->AddRect(ImVec2(ox, oy),
                    ImVec2(ox + dispW, oy + dispH),
                    IM_COL32(180, 180, 180, 200));

        // Translucent scrollbars when image overflows
        float barThick = 8.0f;
        float barPad = 2.0f;
        ImU32 barCol = IM_COL32(200, 200, 200, 100);
        ImU32 barColHov = IM_COL32(200, 200, 200, 180);
        ImVec2 mpos = io.MousePos;

        if (dispW > avail.x)
        {
            float viewRatio = avail.x / dispW;
            float barW = avail.x * viewRatio;
            // Negative pan = image left, viewport right
            float t =
                (limX - pan.x) / (2.0f * limX);
            float barX = canvasPos.x
                + t * (avail.x - barW);
            float barY = canvasPos.y + avail.y
                - barThick - barPad;
            ImVec2 bMin(barX, barY);
            ImVec2 bMax(barX + barW,
                        barY + barThick);

            bool barHov =
                mpos.x >= bMin.x && mpos.x <= bMax.x
                && mpos.y >= bMin.y
                && mpos.y <= bMax.y;
            bool barDrag = barHov
                && ImGui::IsMouseDragging(
                    ImGuiMouseButton_Left);
            if (barDrag)
                pan.x -= io.MouseDelta.x
                    / (avail.x - barW)
                    * (2.0f * limX);
            dl->AddRectFilled(
                bMin, bMax,
                barDrag || barHov ? barColHov : barCol,
                barThick * 0.5f);
        }

        if (dispH > avail.y)
        {
            float viewRatio = avail.y / dispH;
            float barH = avail.y * viewRatio;
            float t =
                (limY - pan.y) / (2.0f * limY);
            float barX = canvasPos.x + avail.x
                - barThick - barPad;
            float barY = canvasPos.y
                + t * (avail.y - barH);
            ImVec2 bMin(barX, barY);
            ImVec2 bMax(barX + barThick,
                        barY + barH);

            bool barHov =
                mpos.x >= bMin.x && mpos.x <= bMax.x
                && mpos.y >= bMin.y
                && mpos.y <= bMax.y;
            bool barDrag = barHov
                && ImGui::IsMouseDragging(
                    ImGuiMouseButton_Left);
            if (barDrag)
                pan.y -= io.MouseDelta.y
                    / (avail.y - barH)
                    * (2.0f * limY);
            dl->AddRectFilled(
                bMin, bMax,
                barDrag || barHov ? barColHov : barCol,
                barThick * 0.5f);
        }

        // Re-clamp after scrollbar drag
        pan.x = std::clamp(pan.x, -limX, limX);
        pan.y = std::clamp(pan.y, -limY, limY);

        dl->PopClipRect();
    }

    static void renderImageViewer(AppState& state)
    {
        float totalW = ImGui::GetContentRegionAvail().x;
        float splitterW = 8.0f;
        float leftW =
            totalW * state.viewerSplitRatio - splitterW * 0.5f;
        float rightW =
            totalW * (1.0f - state.viewerSplitRatio)
            - splitterW * 0.5f;

        ImGui::BeginChild("LeftViewer",
                          ImVec2(leftW, 0),
                          ImGuiChildFlags_Borders);
        renderSingleViewer(
            state, state.viewerLeftIdx,
            state.viewerRightIdx,
            state.zoomLeft, state.panLeft, "##Left");
        ImGui::EndChild();

        ImGui::SameLine();

        // Draggable splitter
        float height = ImGui::GetContentRegionAvail().y;
        ImGui::Button("##Splitter",
                      ImVec2(splitterW, height));
        if (ImGui::IsItemActive())
        {
            float delta = ImGui::GetIO().MouseDelta.x;
            state.viewerSplitRatio += delta / totalW;
            state.viewerSplitRatio = std::clamp(
                state.viewerSplitRatio, 0.1f, 0.9f);
        }
        if (ImGui::IsItemHovered()
            || ImGui::IsItemActive())
            ImGui::SetMouseCursor(
                ImGuiMouseCursor_ResizeEW);

        ImGui::SameLine();

        ImGui::BeginChild("RightViewer",
                          ImVec2(rightW, 0),
                          ImGuiChildFlags_Borders);
        renderSingleViewer(
            state, state.viewerRightIdx,
            state.viewerLeftIdx,
            state.zoomRight, state.panRight,
            "##Right");
        ImGui::EndChild();
    }

    static void renderGui(AppState& state)
    {
        if (ImGui::BeginTabBar("MainTabs"))
        {
            if (ImGui::BeginTabItem(
                    "Files & Settings"))
            {
                renderFilesAndSettings(state);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Image Viewer"))
            {
                renderImageViewer(state);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }

    void submain(void)
    {
        runSplash(2.0);

        AppState state;

        HelloImGui::RunnerParams params;
        params.appWindowParams.windowTitle = "ShoeComp";
        params.appWindowParams.windowGeometry
            .fullScreenMode = HelloImGui::
                FullScreenMode::FullMonitorWorkArea;
        params.imGuiWindowParams
            .defaultImGuiWindowType = HelloImGui::
                DefaultImGuiWindowType::
                    ProvideFullScreenWindow;
        params.callbacks.PostInit = []()
        { ImGui::GetIO().FontGlobalScale = 2.5f; };
        params.callbacks.ShowGui =
            [&state]() { renderGui(state); };
        params.callbacks.BeforeExit = [&state]()
        {
            for (auto& img : state.images)
                freeTexture(img.textureId);
            state.images.clear();
        };

        HelloImGui::Run(params);
    }

} // namespace shoecomp
