#ifndef SHOECOMP_ALIGN_DIALOG
#define SHOECOMP_ALIGN_DIALOG

#include <memory>
#include <string>
#include <thread>
#include <vector>
#include "calc/workerChannel.h"
#include "calc/align.h"

namespace shoecomp
{
    struct ImageData;
    enum class AlignMode
    {
        Manual = 0,
        Automatic = 1
    };

    struct AlignResult
    {
        float rotation = 0.0f;
        float translationX = 0.0f;
        float translationY = 0.0f;
        float scale = 1.0f;
    };

    struct AlignState
    {
        AlignMode mode = AlignMode::Manual;
        float rotation = 0.0f;
        float translationX = 0.0f;
        float translationY = 0.0f;
        float scale = 1.0f;
    };

    enum class AlignDialogResult
    {
        None,
        Add,
        Replace
    };

    struct AlignDialog
    {
        bool show = false;
        bool open = false;
        std::string leftName;
        std::string rightName;
        std::shared_ptr<ImageData> leftImage;
        std::shared_ptr<ImageData> rightImage;
        AlignMode mode = AlignMode::Manual;
        AlignResult result;
        WorkerChannel channel;
        std::thread workerThread;
        bool workerFinished = false;

        std::vector<AlignState>* alignments = nullptr;
        int* alignmentIdx = nullptr;

        AlignDialogResult render();
        void startWorker();
        void cancelWorker();
        void cleanup();
    };
}  // namespace shoecomp

#endif
