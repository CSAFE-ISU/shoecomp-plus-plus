#ifndef SHOECOMP_MAIN_WINDOW
#define SHOECOMP_MAIN_WINDOW

#include "hello_imgui/hello_imgui.h"
#include "imgui.h"
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include "json.h"
#include "ui/errorPopup.h"
#include "ui/loadBrowser.h"
#include "ui/saveBrowser.h"
#include "ui/imageCanvas.h"
#include "ui/imageListDialog.h"

namespace shoecomp
{
    struct AppState
    {
        // Loaded images
        std::vector<ImageCanvas> images;

        // Image viewer selections (-1 = none)
        int viewerLeftIdx = -1;
        int viewerRightIdx = -1;
        float viewerSplitRatio = 0.5f;

        // Per-viewer canvas (own view state,
        // shared image data)
        ImageCanvas viewerLeft;
        ImageCanvas viewerRight;
        bool viewerLocked = false;

        // Active gallery image (last-focused
        // window, -1 = none)
        int activeGalleryImage = -1;

        // Dialogs
        LoadBrowser imageLoadBrowser;
        SaveBrowser imageSaveBrowser;
        SaveBrowser annotationFileBrowser;
        ImageListDialog imageListDialog;

        // Error popups
        ErrorPopup imageSaveError;
        ErrorPopup annotationError;

        // Annotation file browser mode
        bool annotationFileSave = false;
        int annotationFileTarget = -1;

        // Image save target
        int imageSaveTarget = -1;

        // Image save background thread
        std::atomic<bool> imageSaveInProgress{false};
        std::atomic<bool> imageSaveDone{false};
        std::atomic<int> imageSaveResult{0};
        std::string imageSaveProgressPath;
        std::thread imageSaveThread;
    };

    void submain(void);
} /* namespace shoecomp */

#endif
