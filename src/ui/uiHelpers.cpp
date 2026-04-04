#include "ui/uiHelpers.h"
#include <algorithm>
#include <filesystem>

namespace shoecomp
{
    namespace fs = std::filesystem;

    bool popupBeginClosable(const char* title, bool& show, float wRatio,
                            float hRatio, float xOff, float yOff)
    {
        if (show)
        {
            ImGui::OpenPopup(title);
            show = false;
        }
        ImVec2 ds = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowSize(ImVec2(ds.x * wRatio, ds.y * hRatio),
                                 ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(ds.x * xOff, ds.y * yOff),
                                ImGuiCond_Always);
        bool open = true;
        if (!ImGui::BeginPopupModal(title, &open,
                                    ImGuiWindowFlags_NoResize))
            return false;
        if (!open)
        {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return false;
        }
        return true;
    }

    bool hasAnnotationArray(jt::Json& annotations, const char* key)
    {
        return annotations.isObject() && annotations.contains(key) &&
               annotations[key].isArray();
    }

    void settingsTableRow(const char* label)
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%s", label);
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
    }

    void clampViewerIndices(int removedIdx, int imageCount,
                            int& leftIdx, int& rightIdx, int& activeIdx)
    {
        (void)removedIdx;
        if (leftIdx >= imageCount) leftIdx = imageCount - 1;
        if (rightIdx >= imageCount) rightIdx = imageCount - 1;
        if (activeIdx == removedIdx)
            activeIdx = -1;
        else if (activeIdx > removedIdx)
            activeIdx--;
    }

    void refreshDirEntries(const std::string& dir,
                           const std::string& extension,
                           std::vector<std::string>& entries,
                           bool& needsRefresh)
    {
        if (!needsRefresh) return;
        entries.clear();
        entries.push_back("..");
        try
        {
            for (auto& entry : fs::directory_iterator(dir))
            {
                std::string name = entry.path().filename().string();
                if (entry.is_directory())
                    entries.push_back(name + "/");
                else if (entry.path().extension() == extension)
                    entries.push_back(name);
            }
        }
        catch (...)
        {
        }
        std::sort(entries.begin() + 1, entries.end());
        needsRefresh = false;
    }

    void navigateDir(std::string& currentDir,
                     const std::string& relativePath,
                     bool& needsRefresh)
    {
        try
        {
            currentDir =
                fs::canonical(fs::path(currentDir) / relativePath)
                    .string();
        }
        catch (...)
        {
        }
        needsRefresh = true;
    }

}  // namespace shoecomp
