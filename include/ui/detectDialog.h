#ifndef SHOECOMP_DETECT_DIALOG
#define SHOECOMP_DETECT_DIALOG

#include <memory>
#include <string>
#include <thread>
#include <vector>
#include "aligncalc/workerChannel.h"
#include "calc/onnxRuntime.h"
#include "ui/imageCanvas.h"
#include "ui/imageCanvas2d.h"
#include "ui/loadBrowser.h"

namespace shoecomp
{
    // Modal that runs an optional ONNX YOLO-style bbox model on the
    // active image and inserts the detected box centers as annotation
    // points. Mirrors AlignDialog's async pattern: a background worker
    // thread communicates through a WorkerChannel, drained per frame.
    struct DetectDialog
    {
        bool show = false;
        bool open = false;

        // Target image + the detection config for its canvas kind,
        // captured when the dialog is opened.
        std::shared_ptr<ImageCanvas2D::ImageData> targetImage;
        ImageCanvas::DetectionSpec spec;

        // RGBA snapshot of the target image, read back on the main/GL
        // thread at open time (GL readback must stay on the UI thread).
        std::vector<unsigned char> frame;
        int frameW = 0;
        int frameH = 0;

        // Model weights picker (.onnx).
        LoadBrowser weightsBrowser;
        std::string modelPath;

        // Editable thresholds, seeded from spec on open.
        float confThreshold = 0.25f;
        float iouThreshold = 0.45f;

        // Async worker state.
        WorkerChannel channel;
        std::thread workerThread;
        bool workerFinished = false;
        std::vector<Detection> results;  // written by the worker
        int appliedCount = -1;           // -1 until results applied

        char statusText[160]{};
        bool statusIsError = false;

        DetectDialog() { weightsBrowser.title = "Load ONNX Model"; }

        // Opens the dialog for |canvas| (which must supportDetection()
        // and hold an image). Snapshots pixels immediately. Returns
        // false and sets statusText if the readback fails.
        bool openFor(ImageCanvas2D& canvas);

        void render();
        void drainMessages();
        void startInference();
        void cancelInference();
        void cleanup();

       private:
        // Inserts the worker's detections into the target image's
        // annotations (main thread). Sets appliedCount.
        void applyResults();
    };
}  // namespace shoecomp

#endif
