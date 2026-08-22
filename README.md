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

## 3D surface topography (x3p)

Besides 2D raster images, `shoecomp` can load and view **x3p**
surface-topography files (ISO 25178-72 / ISO5436_2) as interactive lit
3D surfaces. Pick **3D surface (x3p)** as the active canvas in *Settings
> Active Canvas*, then load a `.x3p` file.

- **Supported layout:** the common **SUR** case — incremental X/Y axes
  with an absolute Z axis, so the height data is a dense matrix. Float
  (`F`/`D`) and integer (`I`/`L`) data types are read; invalid samples
  become holes. Non-conforming files fail with a clear message rather
  than a guess.
- **View:** a height-colormapped (viridis), Lambert-lit mesh with an
  orbit camera — **drag** to orbit, **wheel** to zoom,
  **Ctrl/Right/Middle-drag** to pan. Place **Peak** points and **bounds**
  polygons by clicking the surface (Shift+click removes).
- **Annotations** save to JSON with both the physical coordinates
  (metres, `{x,y,z}`) and the grid index (`{col,row}`, the re-load
  anchor).
- **Comparison:** load two surfaces side by side, **Lock** to sync the
  orbit cameras, and adjust a **manual alignment** (translate / rotate /
  scale) so the two register; alignments save to JSON. *Automatic 3D
  registration is planned future work.*
- **Save PNG** captures the current 3D view.

The 3D **view** requires a desktop **OpenGL** backend (the default on
Windows / macOS / Linux). The **web (Emscripten) build does not offer the
3D surface kind** — it is omitted from the Active Canvas list, since the
renderer's desktop-GL shaders and depth-buffer picking are unavailable on
WebGL2. On a desktop build using a non-OpenGL backend (Metal, DX11) the
kind is still selectable but the view shows a "3D view not available on
this build" placeholder. The ZIP container is read with the vendored
**zlib** submodule; no new dependencies are added.

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
