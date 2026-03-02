#ifndef SHOECOMP_UI
#define SHOECOMP_UI

#include "hello_imgui/hello_imgui.h"
#include "imgui.h"
#include <string>
#include <vector>

namespace shoecomp
{
    struct LoadedImage
    {
        std::string name;
        std::string path;
        ImTextureID textureId = 0;
        int width = 0;
        int height = 0;
    };

    struct AppState
    {
        // Splash
        bool showSplash = true;
        double splashStartTime = 0.0;
        double splashDuration = 2.0;

        // File browser
        std::string currentDir = ".";
        std::vector<std::string> dirEntries;
        bool dirNeedsRefresh = true;

        // Loaded images
        std::vector<LoadedImage> images;

        // Image viewer selections (-1 = none)
        int viewerLeftIdx = -1;
        int viewerRightIdx = -1;

        // Settings
        float settingZoom = 1.0f;
        bool settingAutoFit = true;
    };

    void submain(void);
} /* namespace shoecomp */

#endif
