#ifndef SHOECOMP_ALIGN_DIALOG
#define SHOECOMP_ALIGN_DIALOG

#include <memory>
#include <string>
#include <thread>
#include <vector>
#include "calc/align.h"
#include "jtjson/json.h"

namespace shoecomp
{
    struct ImageData;
    enum class AlignMode
    {
        Manual = 0,
        Automatic = 1
    };

    struct AlignState
    {
        AlignMode mode = AlignMode::Manual;
        float rotation = 0.0f;
        float translationX = 0.0f;
        float translationY = 0.0f;
        float scale = 1.0f;
        jt::Json info;
    };

    struct AlignDialog
    {
        bool show = false;
        bool open = false;
        std::string leftName;
        std::string rightName;
        std::shared_ptr<ImageData> leftImage;
        std::shared_ptr<ImageData> rightImage;
        std::vector<AlignState> workerResults;
        WorkerChannel channel;
        std::thread workerThread;
        bool workerFinished = false;
        AlignCalc::RTSParams rtsParams{
            0.05, 0.05, 2, 4, 3, 500};
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
