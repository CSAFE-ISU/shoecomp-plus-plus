#ifndef SHOECOMP_LOAD_BROWSER
#define SHOECOMP_LOAD_BROWSER

#include <functional>
#include <string>
#include <vector>

namespace shoecomp
{
    struct LoadBrowser
    {
        bool show = false;
        std::string currentDir = ".";
        std::vector<std::string> dirEntries;
        bool dirNeedsRefresh = true;
        std::string extension = ".png";
        std::string title = "Load Image";
        bool loadCorrespondingJson = true;
        std::function<void(const std::string& path,
                           const std::string& name)>
            onSelect;
        void render();
    };
}  // namespace shoecomp

#endif
