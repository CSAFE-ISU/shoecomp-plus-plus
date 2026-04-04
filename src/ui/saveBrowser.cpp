#include "ui/saveBrowser.h"
#include "imgui.h"
#include <algorithm>
#include <filesystem>

namespace shoecomp
{
    namespace fs = std::filesystem;

    void SaveBrowser::render()
    {
        if (show)
        {
            ImGui::OpenPopup(title.c_str());
            show = false;
        }

        ImVec2 ds = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowSize(ImVec2(ds.x * 0.5f, ds.y * 0.6f),
                                 ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(ds.x * 0.25f, ds.y * 0.2f),
                                ImGuiCond_Always);

        bool browserOpen = true;
        if (!ImGui::BeginPopupModal(title.c_str(), &browserOpen,
                                    ImGuiWindowFlags_NoResize))
            return;
        if (!browserOpen)
        {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }

        ImGui::Text("%s", title.c_str());
        ImGui::Separator();

        ImGui::Text("Directory: %s", browseDir.c_str());

        if (dirNeedsRefresh)
        {
            dirEntries.clear();
            dirEntries.push_back("..");
            try
            {
                for (auto& entry : fs::directory_iterator(browseDir))
                {
                    std::string name = entry.path().filename().string();
                    if (entry.is_directory())
                        dirEntries.push_back(name + "/");
                    else if (entry.path().extension() == extension)
                        dirEntries.push_back(name);
                }
            }
            catch (...)
            {
            }
            std::sort(dirEntries.begin() + 1, dirEntries.end());
            dirNeedsRefresh = false;
        }

        ImVec2 listAvail = ImGui::GetContentRegionAvail();
        float bottomH = ImGui::GetFrameHeightWithSpacing() * 2.0f;
        ImGui::BeginChild("SaveFileList",
                          ImVec2(listAvail.x, listAvail.y - bottomH),
                          ImGuiChildFlags_Borders);
        for (auto& entry : dirEntries)
        {
            bool isDir = entry == ".." || entry.back() == '/';
            if (ImGui::Selectable(entry.c_str(), false))
            {
                if (entry == "..")
                {
                    try
                    {
                        browseDir =
                            fs::canonical(fs::path(browseDir) / "..")
                                .string();
                    }
                    catch (...)
                    {
                    }
                    dirNeedsRefresh = true;
                }
                else if (isDir)
                {
                    std::string dirName =
                        entry.substr(0, entry.size() - 1);
                    try
                    {
                        browseDir =
                            fs::canonical(fs::path(browseDir) / dirName)
                                .string();
                    }
                    catch (...)
                    {
                    }
                    dirNeedsRefresh = true;
                }
                else { fileName = entry; }
            }
        }
        ImGui::EndChild();

        char fnBuf[256];
        snprintf(fnBuf, sizeof(fnBuf), "%s", fileName.c_str());
        if (ImGui::InputText("Filename", fnBuf, sizeof(fnBuf)))
        {
            fileName = fnBuf;
        }

        if (ImGui::Button("OK"))
        {
            if (!fileName.empty() && onOk)
            {
                std::string fullPath = browseDir + "/" + fileName;
                onOk(fullPath);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) { ImGui::CloseCurrentPopup(); }

        ImGui::EndPopup();
    }
}  // namespace shoecomp
