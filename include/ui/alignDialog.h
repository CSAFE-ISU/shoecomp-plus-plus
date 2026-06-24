#ifndef SHOECOMP_ALIGN_DIALOG
#define SHOECOMP_ALIGN_DIALOG

#include <memory>
#include <string>
#include <thread>
#include <vector>
#include "imgui.h"
#include "calc/align.h"
#include "jtjson/json.h"
#include "ui/imageCanvas2d.h"

namespace shoecomp
{
    enum class AlignMode
    {
        Manual = 0,
        Automatic = 1
    };

    struct AlignState
    {
        AlignMode mode = AlignMode::Manual;
        float rotation = 0.0f;
        float dx = 0.0f;
        float dy = 0.0f;
        float scale = 1.0f;
        jt::Json info;

        ImVec2 transformRight2Left(const ImVec2& pt) const;
        ImVec2 transformLeft2Right(const ImVec2& pt) const;
    };

    struct AlignDialog
    {
        bool show = false;
        bool open = false;
        std::string leftName;
        std::string rightName;
        std::shared_ptr<ImageCanvas2D::ImageData> leftImage;
        std::shared_ptr<ImageCanvas2D::ImageData> rightImage;
        std::vector<AlignState> workerResults;
        WorkerChannel channel;
        std::thread workerThread;
        bool workerFinished = false;
        AlignCalc::RTSParams rtsParams;
        char statusText[120]{};
        bool statusIsError = false;

        void render();
        void drainMessages();
        void startWorker();
        void cancelWorker();
        void cleanup();
    };
}  // namespace shoecomp

#endif
