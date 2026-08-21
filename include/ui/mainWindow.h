#ifndef SHOECOMP_MAIN_WINDOW
#define SHOECOMP_MAIN_WINDOW

#include "hello_imgui/hello_imgui.h"
#include "imgui.h"
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include "jtjson/json.h"
#include "ui/errorPopup.h"
#include "ui/loadBrowser.h"
#include "ui/saveBrowser.h"
#include "ui/imageCanvas2d.h"
#include "ui/imageListDialog.h"
#include "ui/alignDialog.h"
#include "ui/detectDialog.h"
#include "ui/settings.h"

namespace shoecomp
{
    enum class Workspace
    {
        Viewer = 0,
        Comparison = 1,
        Settings = 2,
        About = 3
    };

    struct AppState
    {
        // Active view, chosen entirely from the menu bar.
        Workspace workspace = Workspace::Viewer;

        // Loaded images
        std::vector<std::unique_ptr<ImageCanvas>> images;

        // Image viewer selections (-1 = none)
        int viewerLeftIdx = -1;
        int viewerRightIdx = -1;

        // Per-viewer canvas (own view state,
        // shared image data)
        std::unique_ptr<ImageCanvas> viewerLeft;
        std::unique_ptr<ImageCanvas> viewerRight;

        // Prototype canvas of the active kind; dispatches loading +
        // supplies the file-picker extension list. Rebuilt on tag
        // change. `lastActiveKind` tracks the applied kind so a change
        // in settings triggers the soft reset.
        std::unique_ptr<ImageCanvas2D> activeProto;
        ImageCanvas::Kind lastActiveKind =
            ImageCanvas::Kind::ShoeCanvas;
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
        DetectDialog detectDialog;

        // Error popups
        ErrorPopup imageLoadError;
        ErrorPopup imageSaveError;
        ErrorPopup annotationError;
        ErrorPopup alignmentSaveError;
        ErrorPopup detectError;

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
        ImFont* iconFont = nullptr;     // Material Icons

        // App-wide settings
        SettingsState settings;

        // Preloaded license texts (filled during splash)
        std::vector<std::string> licenseTexts;
    };

    void renderAbout(AppState& state);
    void renderImageComparison(AppState& state);
    void renderLockToggle(bool& locked);
    void renderImageGallery(AppState& state);
    void renderFilesAndSettings(AppState& state);
    void submain(void);
} /* namespace shoecomp */

#endif
