# `shoecomp++`

This is an experimental fully-`C++` version of the [shoecomp][shoecomp]
application, with the user interface implemented using [Dear ImGui][imgui], and
[hello\_imgui][hello1] -- it's just a fork of the [`hello_imgui_template`
repo][hello2].

MIT License. The goal is to explore how one can implement a minimal, fast,
cross-platform C++ GUI application to visualize for shoeprint image alignment
and comparisons.

[shoecomp]: https://github.com/CSAFE-ISU/shoecomp
[imgui]: https://github.com/ocornut/imgui
[hello1]: https://github.com/pthom/hello_imgui/
[hello2]: https://github.com/pthom/hello_imgui_template/

## Optional automatic point detection (ONNX Runtime)

`shoecomp` can run a YOLO-style object-detection model on the active
image and drop each detected box center in as an annotation point (the
model's class index maps to the point type for the current canvas kind).
This feature is **entirely optional**: the app never builds or links
against ONNX Runtime. Instead it looks for the runtime's shared library
at startup (and whenever you switch canvas kinds) and enables the
**Detect Points** button only if it is found. If the library is absent,
the button stays disabled with a tooltip and nothing else changes.

To enable it:

1. **Build-time (once, by a maintainer):** vendor the ONNX Runtime C-API
   header at `third_party/onnxruntime/onnxruntime_c_api.h`, copied from a
   pinned [onnxruntime][ort] release tag (e.g. `v1.20.1`,
   `include/onnxruntime/core/session/onnxruntime_c_api.h`). Only this
   single header is compiled in.

2. **Run-time (by the user):** download the matching prebuilt runtime for
   your platform from the [onnxruntime releases][ort-rel]
   (`onnxruntime-linux-x64-<ver>.tgz`, `onnxruntime-win-x64-<ver>.zip`,
   or `onnxruntime-osx-<arch>-<ver>.tgz`) and place the shared library
   next to the `shoecomp` executable:
   - Linux: `libonnxruntime.so` (or `libonnxruntime.so.1`)
   - Windows: `onnxruntime.dll`
   - macOS: `libonnxruntime.dylib` (inside
     `shoecomp.app/Contents/Frameworks/`, or set the env var below; a
     downloaded lib may need `xattr -dr com.apple.quarantine`).

   Alternatively, point the `SHOECOMP_ONNXRUNTIME` environment variable
   at the full path of the library.

The app targets the CPU execution provider by default and will opt into a
GPU provider (CUDA / DirectML / CoreML) automatically if the loaded
runtime build provides one, silently falling back to CPU otherwise. If
the installed runtime is older than the vendored header's API version,
the feature simply stays disabled.

[ort]: https://github.com/microsoft/onnxruntime
[ort-rel]: https://github.com/microsoft/onnxruntime/releases
