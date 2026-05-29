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
        bool loadMode = false;
        std::function<void(const std::string& path)> onOk;
        void render();
    };
}  // namespace shoecomp

#endif
