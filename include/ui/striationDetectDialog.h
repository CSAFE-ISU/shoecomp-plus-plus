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
    // Modal that runs geometric striation detection on a bullet-land
    // x3p surface (which must have a cylinder fit) and REPLACES its
    // peaks[] with the detected 5-D regions. Mirrors DetectDialog's
    // async pattern (WorkerChannel worker drained per frame) minus the
    // model browser; detection is pure CPU math (calc/striationDetect).
    struct StriationDetectDialog
    {
        bool show = false;
        bool open = false;

        // Target surface + a stable copy of its cylinder fit, captured
        // when the dialog is opened (worker reads heights + fit only).
        std::shared_ptr<Surface3D> surface;
        CylinderFit fit;

        // Editable thresholds in UI units (converted to SI on run).
        float minThetaDeg = 1.0f;  // min angular WIDTH, degrees
        float minDrUm = 3.0f;      // |dr| threshold, micrometres
        float minLengthMm = 0.3f;  // min axial length, millimetres
        int maxCount = 20;         // keep the strongest N

        // Async worker state.
        WorkerChannel channel;
        std::thread workerThread;
        bool workerFinished = false;
        std::vector<PeakRegion5D> results;  // written by the worker
        int appliedCount = -1;              // -1 until results applied

        char statusText[160]{};
        bool statusIsError = false;

        // Opens the dialog for |canvas| (which must have hasFit and a
        // surface). Returns false and sets statusText otherwise.
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
