#include "ui/loadBrowser.h"
#include "imgui.h"
#include <algorithm>
#include <filesystem>

namespace shoecomp
{
    namespace fs = std::filesystem;

    void LoadBrowser::render()
    {
        if (show)
        {
            ImGui::OpenPopup(title.c_str());
            show = false;
        }

        ImVec2 ds = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowSize(
            ImVec2(ds.x * 0.5f, ds.y * 0.6f),
            ImGuiCond_Always);
        ImGui::SetNextWindowPos(
            ImVec2(ds.x * 0.25f, ds.y * 0.2f),
            ImGuiCond_Always);

        bool browserOpen = true;
        if (!ImGui::BeginPopupModal(
                title.c_str(), &browserOpen,
                ImGuiWindowFlags_NoResize))
            return;
        if (!browserOpen)
        {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }

        ImGui::Text("Directory: %s", currentDir.c_str());
        ImGui::SameLine();
        if (ImGui::Button("Refresh"))
            dirNeedsRefresh = true;

        if (dirNeedsRefresh)
        {
            dirEntries.clear();
            dirEntries.push_back("..");
            try
            {
                for (auto& entry :
                     fs::directory_iterator(currentDir))
                {
                    std::string name =
                        entry.path().filename().string();
                    if (entry.is_directory())
                        dirEntries.push_back(name + "/");
                    else if (entry.path().extension() ==
                             extension)
                        dirEntries.push_back(name);
                }
            }
            catch (...)
            {
            }
            std::sort(dirEntries.begin() + 1,
                      dirEntries.end());
            dirNeedsRefresh = false;
        }

        ImVec2 listAvail =
            ImGui::GetContentRegionAvail();
        float bottomH =
            ImGui::GetFrameHeightWithSpacing();
        ImGui::BeginChild(
            "FileList",
            ImVec2(listAvail.x,
                   listAvail.y - bottomH),
            ImGuiChildFlags_Borders);
        for (auto& entry : dirEntries)
        {
            bool isDir =
                entry == ".." || entry.back() == '/';
            ImGuiSelectableFlags flags =
                isDir
                    ? ImGuiSelectableFlags_None
                    : ImGuiSelectableFlags_AllowDoubleClick;

            if (ImGui::Selectable(entry.c_str(), false,
                                  flags))
            {
                if (entry == "..")
                {
                    try
                    {
                        currentDir =
                            fs::canonical(
                                fs::path(currentDir) /
                                "..")
                                .string();
                    }
                    catch (...)
                    {
                    }
                    dirNeedsRefresh = true;
                }
                else if (entry.back() == '/')
                {
                    std::string dirName = entry.substr(
                        0, entry.size() - 1);
                    try
                    {
                        currentDir =
                            fs::canonical(
                                fs::path(currentDir) /
                                dirName)
                                .string();
                    }
                    catch (...)
                    {
                    }
                    dirNeedsRefresh = true;
                }
                else if (ImGui::IsMouseDoubleClicked(
                             ImGuiMouseButton_Left))
                {
                    std::string fullPath =
                        fs::canonical(
                            fs::path(currentDir) /
                            entry)
                            .string();
                    if (onSelect)
                        onSelect(fullPath, entry);
                    ImGui::CloseCurrentPopup();
                }
            }
        }
        ImGui::EndChild();

        ImGui::Checkbox("Load Corresponding JSON",
                        &loadCorrespondingJson);
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}  // namespace shoecomp
