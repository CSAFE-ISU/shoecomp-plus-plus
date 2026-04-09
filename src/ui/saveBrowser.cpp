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

        {
            char pathBuf[512];
            snprintf(pathBuf, sizeof(pathBuf), "%s",
                     browseDir.c_str());
            bool entered = ImGui::InputText(
                "Path", pathBuf, sizeof(pathBuf),
                ImGuiInputTextFlags_EnterReturnsTrue);
            browseDir = pathBuf;
            if (entered)
            {
                std::error_code ec;
                fs::path p(browseDir);
                if (fs::is_directory(p, ec))
                {
                    browseDir = fs::canonical(p, ec).string();
                    dirNeedsRefresh = true;
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Refresh")) dirNeedsRefresh = true;

        if (!extensionChoices.empty())
        {
            auto labelFor = [](const std::string& e) {
                return e.empty() ? std::string("(all files)") : e;
            };
            std::string preview = labelFor(extension);
            if (ImGui::BeginCombo("Extension", preview.c_str()))
            {
                for (auto& choice : extensionChoices)
                {
                    bool selected = choice == extension;
                    std::string label = labelFor(choice);
                    if (ImGui::Selectable(label.c_str(), selected))
                    {
                        extension = choice;
                        dirNeedsRefresh = true;
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }

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
            if (ImGui::Selectable(
                    entry.c_str(), false,
                    ImGuiSelectableFlags_AllowDoubleClick))
            {
                if (isDir)
                {
                    if (ImGui::IsMouseDoubleClicked(
                            ImGuiMouseButton_Left))
                    {
                        if (entry == "..")
                        {
                            navigateDir(browseDir, "..",
                                        dirNeedsRefresh);
                        }
                        else
                        {
                            std::string dirName = entry.substr(
                                0, entry.size() - 1);
                            navigateDir(browseDir, dirName,
                                        dirNeedsRefresh);
                        }
                    }
                }
                else { fileName = entry; }
            }
        }
        ImGui::EndChild();

        if (!contextLabel.empty())
        {
            ImGui::Text("Saving annotations for: %s",
                        contextLabel.c_str());
        }

        auto finalize = [&]() {
            std::string finalName = fileName;
            if (!extension.empty() &&
                fs::path(finalName).extension() != extension)
                finalName += extension;
            if (!finalName.empty() && onOk)
            {
                std::string fullPath = browseDir + "/" + finalName;
                onOk(fullPath);
            }
            ImGui::CloseCurrentPopup();
        };

        char fnBuf[256];
        snprintf(fnBuf, sizeof(fnBuf), "%s", fileName.c_str());
        bool entered =
            ImGui::InputText("Filename", fnBuf, sizeof(fnBuf),
                             ImGuiInputTextFlags_EnterReturnsTrue);
        fileName = fnBuf;
        if (entered)
        {
            fs::path resolved = fs::path(browseDir) / fileName;
            std::error_code ec;
            if (fs::is_directory(resolved, ec))
            {
                navigateDir(browseDir, fileName, dirNeedsRefresh);
                fileName.clear();
            }
            else
            {
                finalize();
                ImGui::EndPopup();
                return;
            }
        }

        if (ImGui::Button("OK"))
        {
            finalize();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) { ImGui::CloseCurrentPopup(); }

        ImGui::EndPopup();
    }
}  // namespace shoecomp
