#ifndef SHOECOMP_UI
#define SHOECOMP_UI

#include "hello_imgui/hello_imgui.h"
#include "imgui.h"
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include "json.h"

namespace shoecomp
{
    enum class AnnotationMode
    {
        None,
        AddPoint,
        AddBounds
    };

    struct ImageViewState
    {
        float zoom = 1.0f;
        float zoomTarget = 1.0f;
        ImVec2 pan = ImVec2(0, 0);
        ImVec2 panTarget = ImVec2(0, 0);
        float rotation = 0.0f;
        float rotationTarget = 0.0f;
    };

    struct LoadedImage
    {
        std::string name;
        std::string path;
        ImTextureID textureId = 0;
        int width = 0;
        int height = 0;
        ImageViewState viewState;
        jt::Json annotations;
        AnnotationMode annotationMode = AnnotationMode::None;
    };

    struct AppState
    {
        // File browser
        std::string currentDir = ".";
        std::vector<std::string> dirEntries;
        bool dirNeedsRefresh = true;

        // Loaded images
        std::vector<LoadedImage> images;

        // Image viewer selections (-1 = none)
        int viewerLeftIdx = -1;
        int viewerRightIdx = -1;
        float viewerSplitRatio = 0.5f;

        // Per-viewer zoom and pan state
        ImageViewState viewerLeftState;
        ImageViewState viewerRightState;
        bool viewerLocked = false;

        // Annotation file browser dialog
        bool showAnnotationFileBrowser = false;
        bool annotationFileSave = false;
        int annotationFileTarget = -1;
        std::string annotationBrowseDir = ".";
        std::vector<std::string> annotationDirEntries;
        bool annotationDirNeedsRefresh = true;
        std::string annotationFileName;

        // Error popup
        bool showAnnotationError = false;
        std::string annotationErrorMsg;

        // Image save file browser dialog
        bool showImageSaveFileBrowser = false;
        int imageSaveTarget = -1;
        std::string imageSaveBrowseDir = ".";
        std::vector<std::string> imageSaveDirEntries;
        bool imageSaveDirNeedsRefresh = true;
        std::string imageSaveFileName;
        bool showImageSaveError = false;
        std::string imageSaveErrorMsg;

        // Image save progress (background thread)
        std::atomic<bool> imageSaveInProgress{false};
        std::atomic<bool> imageSaveDone{false};
        std::atomic<int> imageSaveResult{0};
        std::string imageSaveProgressPath;
        std::thread imageSaveThread;
    };

    void submain(void);
} /* namespace shoecomp */

#endif
