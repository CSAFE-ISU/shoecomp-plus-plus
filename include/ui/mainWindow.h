#ifndef SHOECOMP_MAIN_WINDOW
#define SHOECOMP_MAIN_WINDOW

#include "hello_imgui/hello_imgui.h"
#include "imgui.h"
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include "jtjson/json.h"
#include "ui/errorPopup.h"
#include "ui/loadBrowser.h"
#include "ui/saveBrowser.h"
#include "ui/imageCanvas.h"
#include "ui/imageListDialog.h"
#include "ui/alignDialog.h"
#include "ui/settings.h"

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
        std::vector<AlignState> viewerAlignments{AlignState{}};
        int viewerAlignmentIdx = 0;

        // Active gallery image (last-focused
        // window, -1 = none)
        int activeGalleryImage = -1;

        // Alignment edit popup
        bool alignEditOpen = false;
        bool alignEditPopupVisible = false;
        AlignState alignEditState;
        AlignState alignEditOriginal;

        // Dialogs
        LoadBrowser imageLoadBrowser;
        SaveBrowser imageSaveBrowser;
        SaveBrowser annotationFileBrowser;
        ImageListDialog imageListDialog;
        AlignDialog alignDialog;

        // Error popups
        ErrorPopup imageSaveError;
        ErrorPopup annotationError;
        ErrorPopup alignmentSaveError;

        // Alignment save
        SaveBrowser alignmentSaveBrowser;

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

        // Custom fonts
        ImFont* defaultFont = nullptr;  // Montserrat-Regular
        ImFont* boldFont = nullptr;     // Montserrat-SemiBold
        ImFont* monoFont = nullptr;     // Inconsolata

        // App-wide settings
        SettingsState settings;

        // Preloaded license texts (filled during splash)
        std::vector<std::string> licenseTexts;
    };

    void applyAlignment(ImageCanvas&, ImageCanvas&,  //
                        const AlignState& a, bool&);
    void syncLockedViewers(ImageCanvas&, ImageCanvas&,
                           const AlignState&,     //
                           float, ImVec2, float,  //
                           float, ImVec2, float);
    void renderLockedCursorIndicators(const AppState&,
                                      const SettingsState&);
    //
    void renderAbout(AppState& state);
    void renderImageComparison(AppState& state);
    void renderSingleViewer(AppState& state, int& selectedIdx,
                            int otherIdx, ImageCanvas& viewer,
                            const char* label);
    void renderLockToggle(bool& locked);
    void renderImageGallery(AppState& state);
    void renderFilesAndSettings(AppState& state);
    void renderSettings(AppState& state);
    //
    void submain(void);
} /* namespace shoecomp */

#endif
