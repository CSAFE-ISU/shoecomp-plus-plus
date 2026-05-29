#include "ui/saveBrowser.h"
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

    void SaveBrowser::render()
    {
        if (!popupBeginClosable(title.c_str(), show, 0.5f, 0.6f, 0.25f,
                                0.2f))
            return;

        ImGui::Text("%s", title.c_str());
        ImGui::Separator();

        {
            char pathBuf[512];
            snprintf(pathBuf, sizeof(pathBuf), "%s", browseDir.c_str());
            bool entered =
                ImGui::InputText("Path", pathBuf, sizeof(pathBuf),
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
                            std::string dirName =
                                entry.substr(0, entry.size() - 1);
                            navigateDir(browseDir, dirName,
                                        dirNeedsRefresh);
                        }
                    }
                }
                else
                {
                    fileName = entry;
                }
            }
        }
        ImGui::EndChild();

        if (!contextLabel.empty())
        {
            ImGui::Text("Saving annotations for: %s",
                        contextLabel.c_str());
        }

        auto finalize = [&]()
        {
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

        if (ImGui::Button("OK")) { finalize(); }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) { ImGui::CloseCurrentPopup(); }

        ImGui::EndPopup();
    }

#else  // __EMSCRIPTEN__

    void SaveBrowser::render() {}

#endif  // __EMSCRIPTEN__

#ifdef __EMSCRIPTEN__

    static SaveBrowser* s_activeSaveBrowser = nullptr;

    extern "C" void EMSCRIPTEN_KEEPALIVE scpp_onJsonFileUploaded(
        const char* name, const uint8_t* data, int size)
    {
        if (!s_activeSaveBrowser) return;
        std::string filename(name);
        std::string path = "/tmp/" + filename;
        FILE* f = fopen(path.c_str(), "wb");
        if (f)
        {
            fwrite(data, 1, (size_t)size, f);
            fclose(f);
        }
        if (s_activeSaveBrowser->onOk) s_activeSaveBrowser->onOk(path);
    }

    EM_JS(void, scpp_emOpenJsonPicker, (), {
        var input = document.createElement("input");
        input.type = "file";
        input.accept = ".json";
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
                var dataPtr = _malloc(data.length);
                HEAPU8.set(data, dataPtr);
                _scpp_onJsonFileUploaded(namePtr, dataPtr, data.length);
                _free(namePtr);
                _free(dataPtr);
            };
            reader.readAsArrayBuffer(file);
        };
        input.click();
    });

    EM_JS(void, scpp_emDownloadString,
          (const char* data, int len, const char* filename,
           int filenameLen),
          {
              var str = UTF8ToString(data, len);
              var fname = UTF8ToString(filename, filenameLen);
              var blob = new Blob([str],
                                  {
                                      type:
                                          "application/json"
                                  });
              var url = URL.createObjectURL(blob);
              var a = document.createElement("a");
              a.href = url;
              a.download = fname;
              document.body.appendChild(a);
              a.click();
              document.body.removeChild(a);
              URL.revokeObjectURL(url);
          });

    void openJsonPicker(SaveBrowser& browser)
    {
        s_activeSaveBrowser = &browser;
        scpp_emOpenJsonPicker();
    }

    void downloadJsonString(const char* data, int len,
                            const char* filename, int filenameLen)
    {
        scpp_emDownloadString(data, len, filename, filenameLen);
    }

#else  // desktop

    void openJsonPicker(SaveBrowser& browser) { browser.show = true; }

    void downloadJsonString(const char* /*data*/, int /*len*/,
                            const char* /*filename*/,
                            int /*filenameLen*/)
    {
    }

#endif  // __EMSCRIPTEN__

}  // namespace shoecomp
