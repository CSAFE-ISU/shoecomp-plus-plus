#ifndef SHOECOMP_CALC_ONNX_RUNTIME_H
#define SHOECOMP_CALC_ONNX_RUNTIME_H

#include <atomic>
#include <string>
#include <vector>

// Optional ONNX Runtime integration.
//
// The onnxruntime shared library is NOT built or linked with the app.
// It is located and dlopen'd at runtime (if present); otherwise every
// entry point here degrades gracefully and the detection feature stays
// disabled. Only the vendored C-API header
// (third_party/onnxruntime/onnxruntime_c_api.h) is compiled in, and
// only inside onnxRuntime.cpp -- no ONNX type appears in this header so
// callers never need the runtime headers.
namespace shoecomp
{
    // Thin manager around the dlopen'd runtime. Single instance;
    // ensureLoaded() performs the one-time search + load.
    class OnnxRuntime
    {
       public:
        static OnnxRuntime& instance();

        // Search for and load the onnxruntime shared library. Cheap and
        // idempotent after the first call. Never throws; returns
        // available(). Safe to call from the UI thread every time the
        // active canvas kind changes.
        bool ensureLoaded();

        // True once the library was found AND its API table resolved
        // for our compiled ORT_API_VERSION.
        bool available() const { return available_; }

        // Full path of the loaded library (empty if none).
        const std::string& libPath() const { return libPath_; }

        // Last diagnostic message (load failure reason, or the selected
        // execution provider on success). Informational.
        const std::string& lastError() const { return lastError_; }

       private:
        OnnxRuntime() = default;
        bool triedLoad_ = false;
        bool available_ = false;
        std::string libPath_;
        std::string lastError_;
    };

    // A single detected box, expressed in ORIGINAL image pixel space
    // (the letterbox transform has already been inverted). (cx, cy) is
    // the box center -- the point that becomes an annotation.
    struct Detection
    {
        float cx = 0.0f;
        float cy = 0.0f;
        float w = 0.0f;
        float h = 0.0f;
        float conf = 0.0f;
        int cls = 0;
    };

    // Inputs for one YOLO-style bbox inference on a single RGBA frame.
    struct DetectionInput
    {
        const unsigned char* rgba = nullptr;  // imgW*imgH*4, top-left
        int imgW = 0;
        int imgH = 0;
        int inputW = 640;       // model input width
        int inputH = 640;       // model input height
        bool nchw = true;       // NCHW (vs NHWC) tensor layout
        bool normalize = true;  // divide by 255
        float confThreshold = 0.25f;
        float iouThreshold = 0.45f;
    };

    // Load |modelPath|, run bbox inference on |in|, and append the
    // surviving detections (post-threshold, post-NMS, mapped to image
    // pixels) to |out|. Returns false and sets |err| on any failure.
    // Requires OnnxRuntime::instance().available(). Does CPU-side work
    // only, so it is safe to call from a worker thread. If |cancel| is
    // non-null and becomes true, the call aborts early with err set to
    // "cancelled".
    bool runYoloDetection(const std::string& modelPath,
                          const DetectionInput& in,
                          std::vector<Detection>& out, std::string& err,
                          const std::atomic<bool>* cancel = nullptr);
}  // namespace shoecomp

#endif
