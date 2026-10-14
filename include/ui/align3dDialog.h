#ifndef SHOECOMP_ALIGN3D_DIALOG
#define SHOECOMP_ALIGN3D_DIALOG

#include <string>
#include <thread>
#include <vector>

#include "aligncalc/workerChannel.h"
#include "calc/align3d.h"
#include "ui/imageCanvas3d.h"

namespace shoecomp
{
    // Modal that registers two bullet lands from their striation lines
    // (calc/align3d). The lines are copied when the dialog opens, so
    // the worker never reads live annotations. Mirrors
    // StriationDetectDialog's async pattern.
    struct Align3DDialog
    {
        bool show = false;
        bool open = false;
        std::string leftName, rightName;

        float angleTolDeg = 0.5f;
        float sepTolUm = 4.0f;
        float maxLengthRatio = 0.0f;  // 0 = off
        int minLines = 10;
        int numResults = 2;
        int numWorkers = 4;

        AlignCalc::DoubleMatrixR leftRows, rightRows;
        std::vector<int> leftPeakIdx, rightPeakIdx;  // row -> peaks[]

        WorkerChannel channel;
        std::thread workerThread;
        bool workerFinished = false;
        std::vector<StriationAlignment> results;  // written by worker

        char statusText[160]{};
        bool statusIsError = false;

        // False with statusText set when either land has too few
        // striations.
        bool openFor(const ImageCanvas3D& left,
                     const ImageCanvas3D& right);
        void render();
        void drainMessages();
        void start();
        void cancel();
        void cleanup();
    };
}  // namespace shoecomp

#endif
