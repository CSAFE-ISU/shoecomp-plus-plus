#ifndef SHOECOMP_STRIATION_DETECT_DIALOG
#define SHOECOMP_STRIATION_DETECT_DIALOG

#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "aligncalc/workerChannel.h"
#include "calc/striationDetect.h"
#include "ui/imageCanvas3d.h"

namespace shoecomp
{
    // Modal that runs 3-D-Hough striation detection on a bullet-land
    // x3p surface and REPLACES its peaks[] with the detected lines.
    // Detection is fit-independent (it works on the raw surface); a
    // cylinder fit is not required. Mirrors DetectDialog's async
    // pattern (WorkerChannel worker drained per frame) minus the model
    // browser; detection is pure CPU math (calc/striationDetect).
    struct StriationDetectDialog
    {
        bool show = false;
        bool open = false;

        // Target surface, captured when the dialog is opened (the
        // worker reads its heights only).
        std::shared_ptr<Surface3D> surface;

        // Editable thresholds in UI units (converted to SI on run).
        float minLengthMm = 0.5f;     // min striation length, mm
        float minHeightUm = 1.0f;     // min |height diff|, micrometres
        float maxSurfDevDeg = 20.0f;  // max deviation from surface, deg
        int maxCount = 20;            // keep the strongest N

        // Async worker state.
        WorkerChannel channel;
        std::thread workerThread;
        bool workerFinished = false;
        std::vector<StriationLine> results;  // written by the worker
        int appliedCount = -1;               // -1 until results applied

        char statusText[160]{};
        bool statusIsError = false;

        // Opens the dialog for |canvas| (needs only a loaded surface).
        // Returns false and sets statusText otherwise.
        bool openFor(X3PBulletLand& canvas);

        void render();
        void drainMessages();
        void startDetection();
        void cancelDetection();
        void cleanup();

       private:
        // Replaces the target surface's peaks[] with the worker's
        // results (main thread). Sets appliedCount.
        void applyResults();
    };
}  // namespace shoecomp

#endif
