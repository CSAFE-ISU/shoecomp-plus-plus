#include "ui/loadBrowser.h"
#include "ui/uiHelpers.h"
#include "imgui.h"
#include <filesystem>

namespace shoecomp
{
    namespace fs = std::filesystem;

    void LoadBrowser::render()
    {
        if (!popupBeginClosable(title.c_str(), show, 0.5f, 0.6f, 0.25f,
                                0.2f))
            return;

        ImGui::Text("Directory: %s", currentDir.c_str());
        ImGui::SameLine();
        if (ImGui::Button("Refresh")) dirNeedsRefresh = true;

        refreshDirEntries(currentDir, extension, dirEntries,
                          dirNeedsRefresh);

        ImVec2 listAvail = ImGui::GetContentRegionAvail();
        float bottomH = ImGui::GetFrameHeightWithSpacing();
        ImGui::BeginChild("FileList",
                          ImVec2(listAvail.x, listAvail.y - bottomH),
                          ImGuiChildFlags_Borders);
        for (auto& entry : dirEntries)
        {
            bool isDir = entry == ".." || entry.back() == '/';
            ImGuiSelectableFlags flags =
                isDir ? ImGuiSelectableFlags_None
                      : ImGuiSelectableFlags_AllowDoubleClick;

            if (ImGui::Selectable(entry.c_str(), false, flags))
            {
                if (entry == "..")
                {
                    navigateDir(currentDir, "..", dirNeedsRefresh);
                }
                else if (entry.back() == '/')
                {
                    std::string dirName =
                        entry.substr(0, entry.size() - 1);
                    navigateDir(currentDir, dirName, dirNeedsRefresh);
                }
                else if (ImGui::IsMouseDoubleClicked(
                             ImGuiMouseButton_Left))
                {
                    std::string fullPath =
                        fs::canonical(fs::path(currentDir) / entry)
                            .string();
                    if (onSelect) onSelect(fullPath, entry);
                    ImGui::CloseCurrentPopup();
                }
            }
        }
        ImGui::EndChild();

        ImGui::Checkbox("Load Corresponding JSON",
                        &loadCorrespondingJson);
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) { ImGui::CloseCurrentPopup(); }

        ImGui::EndPopup();
    }
}  // namespace shoecomp
