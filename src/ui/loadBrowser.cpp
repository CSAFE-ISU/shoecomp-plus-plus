#include "ui/loadBrowser.h"
#include "ui/uiHelpers.h"
#include "imgui.h"

#ifndef __EMSCRIPTEN__
#include <filesystem>
#else
#include <emscripten.h>
#include <cstdio>
#include <cstring>
#endif

namespace shoecomp
{

#ifndef __EMSCRIPTEN__

    namespace fs = std::filesystem;

    void LoadBrowser::render()
    {
        if (!popupBeginClosable(title.c_str(), show, 0.5f, 0.6f, 0.25f,
                                0.2f))
            return;

        {
            char pathBuf[512];
            snprintf(pathBuf, sizeof(pathBuf), "%s",
                     currentDir.c_str());
            bool entered =
                ImGui::InputText("Path", pathBuf, sizeof(pathBuf),
                                 ImGuiInputTextFlags_EnterReturnsTrue);
            currentDir = pathBuf;
            if (entered)
            {
                std::error_code ec;
                fs::path p(currentDir);
                if (fs::is_directory(p, ec))
                {
                    currentDir = fs::canonical(p, ec).string();
                    dirNeedsRefresh = true;
                }
                else if (fs::exists(p, ec))
                {
                    std::string fullPath =
                        fs::canonical(p, ec).string();
                    std::string name = p.filename().string();
                    currentDir = p.parent_path().string();
                    if (onSelect) onSelect(fullPath, name);
                    ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                    return;
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Refresh")) dirNeedsRefresh = true;

        if (!extensionChoices.empty())
        {
            auto labelFor = [](const std::string& e)
            { return e.empty() ? std::string("(all files)") : e; };
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
        else
        {
            char extBuf[64];
            snprintf(extBuf, sizeof(extBuf), "%s", extension.c_str());
            if (ImGui::InputText("Extension", extBuf, sizeof(extBuf)))
            {
                extension = extBuf;
                dirNeedsRefresh = true;
            }
        }

        refreshDirEntries(currentDir, extension, dirEntries,
                          dirNeedsRefresh);

        ImVec2 listAvail = ImGui::GetContentRegionAvail();
        float bottomH = ImGui::GetFrameHeightWithSpacing();
        ImGui::BeginChild("FileList",
                          ImVec2(listAvail.x, listAvail.y - bottomH),
                          ImGuiChildFlags_Borders);
        for (auto& entry : dirEntries)
        {
            ImGuiSelectableFlags flags =
                ImGuiSelectableFlags_AllowDoubleClick;

            if (ImGui::Selectable(entry.c_str(), false, flags) &&
                ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
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
                else
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

#else  // __EMSCRIPTEN__

    void LoadBrowser::render() {}

#endif  // __EMSCRIPTEN__

#ifdef __EMSCRIPTEN__

    static LoadBrowser* s_activeLoadBrowser = nullptr;
    static LoadBrowser* s_hoveredLoadBrowser = nullptr;

    extern "C" void EMSCRIPTEN_KEEPALIVE scpp_onImageFileUploaded(
        const char* name, const uint8_t* data, int size)
    {
        if (!s_activeLoadBrowser) return;
        std::string filename(name);
        std::string path = "/tmp/" + filename;
        FILE* f = fopen(path.c_str(), "wb");
        if (f)
        {
            fwrite(data, 1, (size_t)size, f);
            fclose(f);
        }
        if (s_activeLoadBrowser->onSelect)
            s_activeLoadBrowser->onSelect(path, filename);
    }

    extern "C" void EMSCRIPTEN_KEEPALIVE scpp_activateLoadBrowser()
    {
        s_activeLoadBrowser = s_hoveredLoadBrowser;
    }

    void setLoadBrowserHovered(LoadBrowser* browser)
    {
        s_hoveredLoadBrowser = browser;
        EM_ASM({ Module.scpp_loadButtonHovered = true; });
    }

    void openImagePicker(LoadBrowser& browser)
    {
        s_activeLoadBrowser = &browser;
    }

#else  // desktop

    void openImagePicker(LoadBrowser& browser) { browser.show = true; }

#endif  // __EMSCRIPTEN__

}  // namespace shoecomp
