#include "ui/loadBrowser.h"
#include "ui/uiHelpers.h"
#include "imgui.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <cstdio>
#include <cstring>
#else
#include <filesystem>
#endif

namespace shoecomp
{

#ifdef __EMSCRIPTEN__

    static LoadBrowser* s_activeLoadBrowser = nullptr;

    // Called from JS when the user selects a file via the
    // browser's native file picker.
    extern "C" void EMSCRIPTEN_KEEPALIVE
    scpp_onFileUploaded(const char* name, const char* mime,
                        const uint8_t* data, int size)
    {
        (void)mime;
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

        s_activeLoadBrowser->show = false;
        s_activeLoadBrowser = nullptr;
    }

    EM_JS(void, scpp_openFilePicker, (const char* accept), {
        var acceptStr = UTF8ToString(accept);
        var input = document.createElement("input");
        input.type = "file";
        input.accept = acceptStr;
        input.onchange = function(e)
        {
            var file = e.target.files[0];
            if (!file) return;
            var reader = new FileReader();
            reader.onload = function()
            {
                var data = new Uint8Array(reader.result);
                var nameLen = lengthBytesUTF8(file.name) + 1;
                var namePtr = _malloc(nameLen);
                stringToUTF8(file.name, namePtr, nameLen);
                var mimeLen = lengthBytesUTF8(file.type) + 1;
                var mimePtr = _malloc(mimeLen);
                stringToUTF8(file.type, mimePtr, mimeLen);
                var dataPtr = _malloc(data.length);
                HEAPU8.set(data, dataPtr);
                _scpp_onFileUploaded(namePtr, mimePtr, dataPtr,
                                     data.length);
                _free(namePtr);
                _free(mimePtr);
                _free(dataPtr);
            };
            reader.readAsArrayBuffer(file);
        };
        input.click();
    });

    void LoadBrowser::render()
    {
        if (!popupBeginClosable(title.c_str(), show, 0.5f, 0.6f, 0.25f,
                                0.2f))
            return;

        if (ImGui::Button("Load Image..."))
        {
            s_activeLoadBrowser = this;
            scpp_openFilePicker(".png,.jpg,.jpeg");
        }

        ImGui::Checkbox("Load Corresponding JSON",
                        &loadCorrespondingJson);
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) { ImGui::CloseCurrentPopup(); }

        ImGui::EndPopup();
    }

#else  // Desktop

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

#endif  // __EMSCRIPTEN__

}  // namespace shoecomp
