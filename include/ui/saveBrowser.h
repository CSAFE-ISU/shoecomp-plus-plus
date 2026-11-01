#ifndef SHOECOMP_SAVE_BROWSER
#define SHOECOMP_SAVE_BROWSER

#include <functional>
#include <string>
#include <vector>

namespace shoecomp
{
    struct SaveBrowser
    {
        bool show = false;
        std::string browseDir = ".";
        std::vector<std::string> dirEntries;
        bool dirNeedsRefresh = true;
        std::string fileName;
        std::string extension;
        std::vector<std::string> extensionChoices;
        std::string title;
        std::string contextLabel;
        std::function<void(const std::string& path)> onOk;
        void render();
    };
    void openJsonPicker(SaveBrowser& browser);
    void downloadJsonString(const char* data, int len,
                            const char* filename, int filenameLen);
    // Web: offer the MEMFS file at path as a download named name
    // (false if it cannot be read). Desktop: no-op, false.
    bool downloadMemfsFile(const std::string& path,
                           const std::string& name);
}  // namespace shoecomp

#endif
