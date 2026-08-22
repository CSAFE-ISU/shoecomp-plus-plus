#include "calc/onnxRuntime.h"

#ifdef __EMSCRIPTEN__

// The web (Emscripten) build cannot load a native onnxruntime shared
// library at runtime (no dlopen/LoadLibrary, no /proc/self/exe), so the
// detection feature is compiled out entirely and every entry point
// reports the runtime as unavailable.
namespace shoecomp
{
    OnnxRuntime& OnnxRuntime::instance()
    {
        static OnnxRuntime inst;
        return inst;
    }

    bool OnnxRuntime::ensureLoaded()
    {
        triedLoad_ = true;
        available_ = false;
        lastError_ = "web version does not support onnxRuntime";
        return false;
    }

    bool runYoloDetection(const std::string&, const DetectionInput&,
                          std::vector<Detection>&, std::string& err,
                          const std::atomic<bool>*)
    {
        err = "web version does not support onnxRuntime";
        return false;
    }
}  // namespace shoecomp

#else  // !__EMSCRIPTEN__

#include "onnxruntime/onnxruntime_c_api.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#if defined(__linux__)
#include <limits.h>
#include <unistd.h>
#endif

namespace shoecomp
{
    namespace
    {
        // The one loaded library handle + resolved API table. There is
        // exactly one runtime, so file-local statics keep the public
        // class free of any ONNX types.
        void* g_lib = nullptr;
        const OrtApi* g_ort = nullptr;

        // --- Platform dynamic-loading shims ---
        void* dlOpen(const char* path)
        {
#ifdef _WIN32
            int n =
                MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);
            std::wstring w(n > 0 ? n - 1 : 0, L'\0');
            if (n > 0)
                MultiByteToWideChar(CP_UTF8, 0, path, -1, &w[0], n);
            return (void*)LoadLibraryW(w.c_str());
#else
            return dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
        }

        void* dlSym(void* h, const char* name)
        {
#ifdef _WIN32
            return (void*)GetProcAddress((HMODULE)h, name);
#else
            return dlsym(h, name);
#endif
        }

        // Directory containing the running executable, with a trailing
        // separator. Empty on failure.
        std::string exeDir()
        {
            std::string p;
#ifdef _WIN32
            wchar_t buf[MAX_PATH];
            DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
            if (n > 0 && n < MAX_PATH)
            {
                int len = WideCharToMultiByte(
                    CP_UTF8, 0, buf, -1, nullptr, 0, nullptr, nullptr);
                std::string s(len > 0 ? len - 1 : 0, '\0');
                if (len > 0)
                    WideCharToMultiByte(CP_UTF8, 0, buf, -1, &s[0], len,
                                        nullptr, nullptr);
                p = s;
            }
#elif defined(__APPLE__)
            uint32_t size = 0;
            _NSGetExecutablePath(nullptr, &size);
            std::string s(size, '\0');
            if (_NSGetExecutablePath(&s[0], &size) == 0)
                p = s.c_str();  // trim to first NUL
#else
            char buf[PATH_MAX];
            ssize_t n =
                readlink("/proc/self/exe", buf, sizeof(buf) - 1);
            if (n > 0)
            {
                buf[n] = '\0';
                p = buf;
            }
#endif
            size_t slash = p.find_last_of("/\\");
            if (slash == std::string::npos) return std::string();
            return p.substr(0, slash + 1);
        }

        // Candidate library file names for this platform, in priority
        // order.
        std::vector<std::string> libNames()
        {
#ifdef _WIN32
            return {"onnxruntime.dll"};
#elif defined(__APPLE__)
            return {"libonnxruntime.dylib", "libonnxruntime.1.dylib"};
#else
            return {"libonnxruntime.so", "libonnxruntime.so.1"};
#endif
        }
    }  // namespace

    OnnxRuntime& OnnxRuntime::instance()
    {
        static OnnxRuntime inst;
        return inst;
    }

    bool OnnxRuntime::ensureLoaded()
    {
        if (triedLoad_) return available_;
        triedLoad_ = true;

        std::vector<std::string> candidates;
        // 1. Explicit override.
        if (const char* env = std::getenv("SHOECOMP_ONNXRUNTIME"))
        {
            if (env[0]) candidates.push_back(env);
        }
        // 2. Next to the executable.
        std::string dir = exeDir();
        for (const auto& n : libNames())
        {
            if (!dir.empty()) candidates.push_back(dir + n);
        }
        // 3. Loader search path (system-installed).
        for (const auto& n : libNames()) candidates.push_back(n);

        for (const auto& cand : candidates)
        {
            void* h = dlOpen(cand.c_str());
            if (!h) continue;
            using GetApiBaseFn = const OrtApiBase*(ORT_API_CALL*)(void);
            auto getBase = (GetApiBaseFn)dlSym(h, "OrtGetApiBase");
            if (!getBase)
            {
                continue;  // not the runtime we expected
            }
            const OrtApiBase* base = getBase();
            const OrtApi* api =
                base ? base->GetApi(ORT_API_VERSION) : nullptr;
            if (!api)
            {
                // Library present but too old for our header version.
                lastError_ =
                    "onnxruntime found but its API version is "
                    "incompatible: " +
                    cand;
                continue;
            }
            g_lib = h;
            g_ort = api;
            libPath_ = cand;
            available_ = true;
            lastError_ = "loaded " + cand;
            return true;
        }

        if (lastError_.empty())
            lastError_ = "onnxruntime shared library not found";
        return false;
    }

    namespace
    {
        // Bilinear sample of channel |c| (0..2) from an RGBA image at
        // fractional (fx, fy), clamped to bounds.
        float sampleRGBA(const unsigned char* rgba, int w, int h,
                         float fx, float fy, int c)
        {
            if (fx < 0) fx = 0;
            if (fy < 0) fy = 0;
            if (fx > w - 1) fx = (float)(w - 1);
            if (fy > h - 1) fy = (float)(h - 1);
            int x0 = (int)fx, y0 = (int)fy;
            int x1 = std::min(x0 + 1, w - 1);
            int y1 = std::min(y0 + 1, h - 1);
            float dx = fx - x0, dy = fy - y0;
            auto px = [&](int x, int y)
            { return (float)rgba[(y * w + x) * 4 + c]; };
            float top = px(x0, y0) * (1 - dx) + px(x1, y0) * dx;
            float bot = px(x0, y1) * (1 - dx) + px(x1, y1) * dx;
            return top * (1 - dy) + bot * dy;
        }

        struct Letterbox
        {
            float scale;
            float padX;
            float padY;
        };

        // Letterbox-resize the RGBA frame into |dst| (float, size
        // 3*inW*inH) in the requested layout. Returns the transform so
        // model-space coords can be inverted back to image pixels.
        Letterbox preprocess(const DetectionInput& in,
                             std::vector<float>& dst)
        {
            const int inW = in.inputW, inH = in.inputH;
            const float scale =
                std::min((float)inW / in.imgW, (float)inH / in.imgH);
            const int newW = (int)std::round(in.imgW * scale);
            const int newH = (int)std::round(in.imgH * scale);
            const float padX = (inW - newW) * 0.5f;
            const float padY = (inH - newH) * 0.5f;
            const float padVal =
                in.normalize ? 114.0f / 255.0f : 114.0f;
            const float norm = in.normalize ? 1.0f / 255.0f : 1.0f;

            dst.assign((size_t)3 * inW * inH, padVal);
            auto put = [&](int c, int y, int x, float v)
            {
                if (in.nchw)
                    dst[((size_t)c * inH + y) * inW + x] = v;
                else
                    dst[((size_t)y * inW + x) * 3 + c] = v;
            };

            for (int oy = 0; oy < inH; ++oy)
            {
                float sy = (oy - padY + 0.5f) / scale - 0.5f;
                if (oy < padY || oy >= padY + newH) continue;
                for (int ox = 0; ox < inW; ++ox)
                {
                    if (ox < padX || ox >= padX + newW) continue;
                    float sx = (ox - padX + 0.5f) / scale - 0.5f;
                    for (int c = 0; c < 3; ++c)
                        put(c, oy, ox,
                            sampleRGBA(in.rgba, in.imgW, in.imgH, sx,
                                       sy, c) *
                                norm);
                }
            }
            return {scale, padX, padY};
        }

        struct RawBox
        {
            float cx, cy, w, h, conf;
            int cls;
        };

        float iou(const RawBox& a, const RawBox& b)
        {
            float ax0 = a.cx - a.w * 0.5f, ay0 = a.cy - a.h * 0.5f;
            float ax1 = a.cx + a.w * 0.5f, ay1 = a.cy + a.h * 0.5f;
            float bx0 = b.cx - b.w * 0.5f, by0 = b.cy - b.h * 0.5f;
            float bx1 = b.cx + b.w * 0.5f, by1 = b.cy + b.h * 0.5f;
            float ix0 = std::max(ax0, bx0), iy0 = std::max(ay0, by0);
            float ix1 = std::min(ax1, bx1), iy1 = std::min(ay1, by1);
            float iw = std::max(0.0f, ix1 - ix0);
            float ih = std::max(0.0f, iy1 - iy0);
            float inter = iw * ih;
            float uni = a.w * a.h + b.w * b.h - inter;
            return uni > 0 ? inter / uni : 0.0f;
        }

        // Greedy per-class non-max suppression.
        void nms(std::vector<RawBox>& boxes, float iouThr)
        {
            std::sort(boxes.begin(), boxes.end(),
                      [](const RawBox& a, const RawBox& b)
                      { return a.conf > b.conf; });
            std::vector<RawBox> kept;
            std::vector<bool> dead(boxes.size(), false);
            for (size_t i = 0; i < boxes.size(); ++i)
            {
                if (dead[i]) continue;
                kept.push_back(boxes[i]);
                for (size_t j = i + 1; j < boxes.size(); ++j)
                {
                    if (dead[j] || boxes[j].cls != boxes[i].cls)
                        continue;
                    if (iou(boxes[i], boxes[j]) > iouThr)
                        dead[j] = true;
                }
            }
            boxes.swap(kept);
        }
    }  // namespace

    bool runYoloDetection(const std::string& modelPath,
                          const DetectionInput& in,
                          std::vector<Detection>& out, std::string& err,
                          const std::atomic<bool>* cancel)
    {
        if (!OnnxRuntime::instance().available() || !g_ort)
        {
            err = "onnxruntime is not available";
            return false;
        }
        auto cancelled = [&] { return cancel && cancel->load(); };
        auto ortOk = [&](OrtStatus* st) -> bool
        {
            if (st == nullptr) return true;
            err = g_ort->GetErrorMessage(st);
            g_ort->ReleaseStatus(st);
            return false;
        };

        OrtEnv* env = nullptr;
        OrtSessionOptions* opts = nullptr;
        OrtSession* session = nullptr;
        OrtMemoryInfo* memInfo = nullptr;
        OrtValue* inputTensor = nullptr;
        OrtValue* outputTensor = nullptr;
        char* inName = nullptr;
        char* outName = nullptr;
        OrtAllocator* alloc = nullptr;
        bool ok = false;

        // RAII-ish cleanup at every exit.
        auto cleanup = [&]
        {
            if (outputTensor) g_ort->ReleaseValue(outputTensor);
            if (inputTensor) g_ort->ReleaseValue(inputTensor);
            if (memInfo) g_ort->ReleaseMemoryInfo(memInfo);
            if (inName && alloc) g_ort->AllocatorFree(alloc, inName);
            if (outName && alloc) g_ort->AllocatorFree(alloc, outName);
            if (session) g_ort->ReleaseSession(session);
            if (opts) g_ort->ReleaseSessionOptions(opts);
            if (env) g_ort->ReleaseEnv(env);
        };

        if (!ortOk(g_ort->CreateEnv(ORT_LOGGING_LEVEL_WARNING,
                                    "shoecomp", &env)))
        {
            cleanup();
            return false;
        }
        if (!ortOk(g_ort->CreateSessionOptions(&opts)))
        {
            cleanup();
            return false;
        }
        g_ort->SetSessionGraphOptimizationLevel(opts, ORT_ENABLE_ALL);

        // Optional GPU execution providers: try, silently fall back to
        // CPU. These are separately-exported C functions that only
        // exist in GPU-enabled builds, so resolve them by symbol.
#if defined(_WIN32)
        const char* epSym =
            "OrtSessionOptionsAppendExecutionProvider_DML";
#elif defined(__APPLE__)
        const char* epSym =
            "OrtSessionOptionsAppendExecutionProvider_CoreML";
#else
        const char* epSym =
            "OrtSessionOptionsAppendExecutionProvider_CUDA";
#endif
        if (g_lib)
        {
            using EpFn = OrtStatus* (*)(OrtSessionOptions*, int);
            auto ep = (EpFn)dlSym(g_lib, epSym);
            if (ep)
            {
                OrtStatus* st = ep(opts, 0);
                if (st) g_ort->ReleaseStatus(st);  // ignore failure
            }
        }

        if (cancelled())
        {
            err = "cancelled";
            cleanup();
            return false;
        }

#ifdef _WIN32
        int wn = MultiByteToWideChar(CP_UTF8, 0, modelPath.c_str(), -1,
                                     nullptr, 0);
        std::wstring wpath(wn > 0 ? wn - 1 : 0, L'\0');
        if (wn > 0)
            MultiByteToWideChar(CP_UTF8, 0, modelPath.c_str(), -1,
                                &wpath[0], wn);
        OrtStatus* sst =
            g_ort->CreateSession(env, wpath.c_str(), opts, &session);
#else
        OrtStatus* sst = g_ort->CreateSession(env, modelPath.c_str(),
                                              opts, &session);
#endif
        if (!ortOk(sst))
        {
            cleanup();
            return false;
        }

        if (!ortOk(g_ort->GetAllocatorWithDefaultOptions(&alloc)))
        {
            cleanup();
            return false;
        }
        if (!ortOk(g_ort->SessionGetInputName(session, 0, alloc,
                                              &inName)) ||
            !ortOk(g_ort->SessionGetOutputName(session, 0, alloc,
                                               &outName)))
        {
            cleanup();
            return false;
        }

        // Preprocess into the input tensor buffer.
        std::vector<float> input;
        Letterbox lb = preprocess(in, input);
        int64_t shape[4];
        if (in.nchw)
        {
            shape[0] = 1;
            shape[1] = 3;
            shape[2] = in.inputH;
            shape[3] = in.inputW;
        }
        else
        {
            shape[0] = 1;
            shape[1] = in.inputH;
            shape[2] = in.inputW;
            shape[3] = 3;
        }
        if (!ortOk(g_ort->CreateCpuMemoryInfo(OrtDeviceAllocator,
                                              OrtMemTypeCPU, &memInfo)))
        {
            cleanup();
            return false;
        }
        if (!ortOk(g_ort->CreateTensorWithDataAsOrtValue(
                memInfo, input.data(), input.size() * sizeof(float),
                shape, 4, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
                &inputTensor)))
        {
            cleanup();
            return false;
        }

        if (cancelled())
        {
            err = "cancelled";
            cleanup();
            return false;
        }

        const char* inNames[] = {inName};
        const char* outNames[] = {outName};
        const OrtValue* inputs[] = {inputTensor};
        if (!ortOk(g_ort->Run(session, nullptr, inNames, inputs, 1,
                              outNames, 1, &outputTensor)))
        {
            cleanup();
            return false;
        }

        // Inspect output shape + element type.
        OrtTensorTypeAndShapeInfo* info = nullptr;
        if (!ortOk(g_ort->GetTensorTypeAndShape(outputTensor, &info)))
        {
            cleanup();
            return false;
        }
        ONNXTensorElementDataType elemType;
        g_ort->GetTensorElementType(info, &elemType);
        size_t nDims = 0;
        g_ort->GetDimensionsCount(info, &nDims);
        std::vector<int64_t> dims(nDims);
        if (nDims) g_ort->GetDimensions(info, dims.data(), nDims);
        g_ort->ReleaseTensorTypeAndShapeInfo(info);

        if (elemType != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT)
        {
            err = "unsupported model output type (expected float)";
            cleanup();
            return false;
        }

        // Collapse to two meaningful dims [A, B].
        std::vector<int64_t> sq;
        for (int64_t d : dims)
            if (d != 1) sq.push_back(d);
        if (sq.size() != 2)
        {
            err =
                "unsupported model output shape (expected a single "
                "bbox tensor)";
            cleanup();
            return false;
        }

        float* data = nullptr;
        if (!ortOk(g_ort->GetTensorMutableData(outputTensor,
                                               (void**)&data)))
        {
            cleanup();
            return false;
        }

        const int A = (int)sq[0], B = (int)sq[1];
        // The attribute axis is the smaller one (5/6/4+numClasses);
        // detections are the large axis.
        bool attrMajor = A <= B;
        int nAttr = attrMajor ? A : B;
        int nDet = attrMajor ? B : A;
        auto at = [&](int det, int attr) -> float
        {
            return attrMajor ? data[(size_t)attr * nDet + det]
                             : data[(size_t)det * nAttr + attr];
        };
        if (nAttr < 5)
        {
            err =
                "unsupported model output layout (need >=5 "
                "attributes per box)";
            cleanup();
            return false;
        }

        // Detect normalized coordinates (all centers <= ~1).
        float maxCoord = 0.0f;
        int probe = std::min(nDet, 64);
        for (int k = 0; k < probe; ++k)
        {
            maxCoord = std::max(maxCoord, std::fabs(at(k, 0)));
            maxCoord = std::max(maxCoord, std::fabs(at(k, 1)));
        }
        float coordScaleX = maxCoord <= 1.5f ? (float)in.inputW : 1.0f;
        float coordScaleY = maxCoord <= 1.5f ? (float)in.inputH : 1.0f;

        const int nClasses = nAttr > 6 ? nAttr - 4 : 1;
        std::vector<RawBox> boxes;
        const int kMaxDet = 8192;
        for (int k = 0; k < nDet && (int)boxes.size() < kMaxDet; ++k)
        {
            float conf;
            int cls;
            if (nAttr == 5)
            {
                conf = at(k, 4);
                cls = 0;
            }
            else if (nAttr == 6)
            {
                conf = at(k, 4);
                cls = (int)std::lround(at(k, 5));
            }
            else  // nAttr > 6: 4 box coords + per-class scores
            {
                conf = -1.0f;
                cls = 0;
                for (int c = 0; c < nClasses; ++c)
                {
                    float s = at(k, 4 + c);
                    if (s > conf)
                    {
                        conf = s;
                        cls = c;
                    }
                }
            }
            if (conf < in.confThreshold) continue;
            RawBox b;
            b.cx = at(k, 0) * coordScaleX;
            b.cy = at(k, 1) * coordScaleY;
            b.w = at(k, 2) * coordScaleX;
            b.h = at(k, 3) * coordScaleY;
            b.conf = conf;
            b.cls = cls;
            boxes.push_back(b);
        }

        nms(boxes, in.iouThreshold);

        // Map surviving boxes from model space back to image pixels.
        for (const auto& b : boxes)
        {
            Detection d;
            d.cx = (b.cx - lb.padX) / lb.scale;
            d.cy = (b.cy - lb.padY) / lb.scale;
            d.w = b.w / lb.scale;
            d.h = b.h / lb.scale;
            d.conf = b.conf;
            d.cls = b.cls;
            if (d.cx < 0 || d.cy < 0 || d.cx > in.imgW ||
                d.cy > in.imgH)
                continue;
            out.push_back(d);
        }

        ok = true;
        cleanup();
        (void)ok;
        return true;
    }
}  // namespace shoecomp

#endif  // __EMSCRIPTEN__
