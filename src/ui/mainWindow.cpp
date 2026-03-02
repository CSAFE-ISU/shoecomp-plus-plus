#include "ui.h"
#include "formats.h"
#include "hello_imgui/hello_imgui_include_opengl.h"
#include <algorithm>
#include <filesystem>

namespace shoecomp
{
    namespace fs = std::filesystem;

    static void renderSplash(AppState& state)
    {
        if (state.splashStartTime == 0.0)
            state.splashStartTime = ImGui::GetTime();

        ImVec2 winSize = ImGui::GetWindowSize();

        const char* title = "ShoeComp";
        ImVec2 titleSize = ImGui::CalcTextSize(title);
        ImGui::SetCursorPos(
            ImVec2((winSize.x - titleSize.x) * 0.5f,
                   (winSize.y - titleSize.y) * 0.5f - 20.0f));
        ImGui::Text("%s", title);

        const char* subtitle = "Loading...";
        ImVec2 subSize = ImGui::CalcTextSize(subtitle);
        ImGui::SetCursorPos(
            ImVec2((winSize.x - subSize.x) * 0.5f,
                   (winSize.y - subSize.y) * 0.5f + 20.0f));
        ImGui::Text("%s", subtitle);

        double elapsed =
            ImGui::GetTime() - state.splashStartTime;
        if (elapsed >= state.splashDuration)
            state.showSplash = false;
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
            if (ImGui::Selectable(entry.c_str()))
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
                        entry.substr(0, entry.size() - 1);
                    try
                    {
                        state.currentDir =
                            fs::canonical(
                                fs::path(state.currentDir)
                                / dirName)
                                .string();
                    }
                    catch (...)
                    {
                    }
                    state.dirNeedsRefresh = true;
                }
                else
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
                        if (loadPngFromDisk(fullPath,
                                            img.textureId,
                                            img.width,
                                            img.height))
                        {
                            state.images.push_back(img);
                        }
                    }
                }
            }
        }
        ImGui::EndChild();
    }

    static void renderSettings(AppState& state)
    {
        ImGui::SliderFloat("Zoom",
                           &state.settingZoom,
                           0.1f,
                           10.0f);
        ImGui::Checkbox("Auto-fit",
                        &state.settingAutoFit);

        ImGui::Separator();
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
            for (int i = 0;
                 i < (int)state.images.size();
                 ++i)
            {
                bool selected = (i == selectedIdx);
                if (ImGui::Selectable(
                        state.images[i].name.c_str(),
                        selected))
                    selectedIdx = i;
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (selectedIdx >= 0
            && selectedIdx < (int)state.images.size())
        {
            auto& img = state.images[selectedIdx];
            ImVec2 avail = ImGui::GetContentRegionAvail();
            float dispW, dispH;

            if (state.settingAutoFit)
            {
                float scaleX = avail.x / (float)img.width;
                float scaleY =
                    avail.y / (float)img.height;
                float scale = std::min(scaleX, scaleY)
                    * state.settingZoom;
                dispW = img.width * scale;
                dispH = img.height * scale;
            }
            else
            {
                dispW = img.width * state.settingZoom;
                dispH = img.height * state.settingZoom;
            }

            ImGui::Image(img.textureId,
                         ImVec2(dispW, dispH));
        }
    }

    static void renderImageViewer(AppState& state)
    {
        float totalW = ImGui::GetContentRegionAvail().x;
        float halfW = totalW * 0.5f;

        ImGui::BeginChild("LeftViewer",
                          ImVec2(halfW, 0),
                          ImGuiChildFlags_Borders);
        renderSingleViewer(
            state, state.viewerLeftIdx, "##Left");
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("RightViewer",
                          ImVec2(0, 0),
                          ImGuiChildFlags_Borders);
        renderSingleViewer(
            state, state.viewerRightIdx, "##Right");
        ImGui::EndChild();
    }

    static void renderGui(AppState& state)
    {
        if (state.showSplash)
        {
            renderSplash(state);
            return;
        }

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
