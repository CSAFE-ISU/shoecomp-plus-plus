#include "ui/saveBrowser.h"
#include "ui/uiHelpers.h"
#include "imgui.h"
#include <filesystem>

namespace shoecomp
{
    namespace fs = std::filesystem;

    void SaveBrowser::render()
    {
        if (!popupBeginClosable(title.c_str(), show, 0.5f, 0.6f, 0.25f,
                                0.2f))
            return;

        ImGui::Text("%s", title.c_str());
        ImGui::Separator();

        ImGui::Text("Directory: %s", browseDir.c_str());

        refreshDirEntries(browseDir, extension, dirEntries,
                          dirNeedsRefresh);

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
                    navigateDir(browseDir, "..", dirNeedsRefresh);
                }
                else if (isDir)
                {
                    std::string dirName =
                        entry.substr(0, entry.size() - 1);
                    navigateDir(browseDir, dirName, dirNeedsRefresh);
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
