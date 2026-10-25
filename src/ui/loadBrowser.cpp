#include "ui/loadBrowser.h"
#include "ui/uiHelpers.h"
#include "imgui.h"

#ifndef __EMSCRIPTEN__
#include <filesystem>
#else
#include <emscripten.h>
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

    // Web image picker. Browsers open a file dialog only from a user
    // gesture, so a canvas pointerdown listener clicks a hidden
    // <input type=file> when the pointer is over a widget that called
    // setLoadBrowserHovered() this frame. The chosen file goes into
    // MEMFS via FS.writeFile and reaches onSelect by path: JS never
    // touches the wasm heap (a page-script HEAPU8 view goes stale when
    // a -pthread build grows memory) and needs no malloc/free, which
    // current Emscripten no longer exports by default.
    EM_JS_DEPS(scpp_load_browser, "$FS,$ccall,$UTF8ToString");

    static LoadBrowser* s_activeLoadBrowser = nullptr;
    static LoadBrowser* s_hoveredLoadBrowser = nullptr;
    static int s_hoveredFrame = -1;
    static std::string s_pickerAccept;

    extern "C" void EMSCRIPTEN_KEEPALIVE
    scpp_onImageFileUploaded(const char* path, const char* name)
    {
        if (s_activeLoadBrowser && s_activeLoadBrowser->onSelect)
            s_activeLoadBrowser->onSelect(path, name);
    }

    // Called by the pointerdown listener (between frames, so the frame
    // count is still the one the hover was recorded in). Returns the
    // file input's accept list, or null when no picker is hovered.
    extern "C" const char* EMSCRIPTEN_KEEPALIVE
    scpp_activateLoadBrowser()
    {
        if (!s_hoveredLoadBrowser ||
            s_hoveredFrame != ImGui::GetFrameCount())
            return nullptr;
        s_activeLoadBrowser = s_hoveredLoadBrowser;
        s_hoveredLoadBrowser = nullptr;
        s_pickerAccept.clear();
        for (const std::string& e :
             s_activeLoadBrowser->extensionChoices)
        {
            if (e.empty()) continue;
            if (!s_pickerAccept.empty()) s_pickerAccept += ',';
            s_pickerAccept += e;
        }
        if (s_pickerAccept.empty())
            s_pickerAccept = s_activeLoadBrowser->extension;
        return s_pickerAccept.c_str();
    }

    EM_JS(void, scpp_emInstallImagePicker, (), {
        var input = document.createElement("input");
        input.type = "file";
        input.style.display = "none";
        document.body.appendChild(input);
        input.addEventListener(
            "change", function(e) {
                var file = e.target.files[0];
                if (!file) return;
                file.arrayBuffer().then(function(buf) {
                    var path = "/tmp/" + file.name;
                    FS.writeFile(path, new Uint8Array(buf));
                    ccall("scpp_onImageFileUploaded", null,
                          [ "string", "string" ], [ path, file.name ]);
                });
                input.value = "";
            });
        Module["canvas"].addEventListener(
            "pointerdown", function() {
                var accept = _scpp_activateLoadBrowser();
                if (!accept) return;
                input.accept = UTF8ToString(accept);
                input.click();
            });
    });

    void setLoadBrowserHovered(LoadBrowser* browser)
    {
        static const bool installed =
            (scpp_emInstallImagePicker(), true);
        (void)installed;
        s_hoveredLoadBrowser = browser;
        s_hoveredFrame = ImGui::GetFrameCount();
    }

    void openImagePicker(LoadBrowser& browser)
    {
        s_activeLoadBrowser = &browser;
    }

#else  // desktop

    void openImagePicker(LoadBrowser& browser) { browser.show = true; }

#endif  // __EMSCRIPTEN__

}  // namespace shoecomp
