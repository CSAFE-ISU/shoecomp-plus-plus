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

## Command-line tool (`scpp`)

`scpp` runs the same detection and automatic alignment as the GUI
without a window, for scripting, tests and comparing many inputs at
once. It is built alongside `shoecomp` (`-DSHOECOMP_BUILD_CLI=OFF` to
skip it, `-DSHOECOMP_BUILD_GUI=OFF` to build only `scpp`, e.g. on a
headless machine) and is not part of the web build.

```sh
scpp -t shoe left.png right.png          # compare one pair
scpp -t x3p a.x3p b.x3p --json -o ab.json
scpp batch -t x3p lands/ --all-pairs --out-dir results/
scpp batch -t ebts prints/ --pairs pairs.txt --out-dir results/
scpp cross -t x3p bullet1/ bullet2/ --out-dir results/
scpp params > my.ini                     # current defaults, editable
scpp -t x3p a.x3p b.x3p -p my.ini --align3d-sep-tol-um 6
scpp help batch                          # or --help / --help-all
scpp version                             # version + licenses
```

- **Types:** `-t shoe | hw | ebts | x3p`. An EBTS transaction holding
  several images takes `file.ebts@N` (0-based record); in batch mode
  every record becomes an input.
- **Annotations:** 2-D inputs use the sidecar `<stem>.json` next to the
  image (`--left-ann/--right-ann` to override), else EBTS Type-9
  minutiae, else ONNX detection with `--detect2d-model model.onnx`.
  x3p inputs use the sidecar's striations, else detect them.
  Detection skips the groove shoulders at the land edges, found from
  the land's averaged cross-section (`-v` prints where;
  `--striation-shoulder-margin-um` trims further inside,
  `--striation-exclude-shoulders false` keeps them).
  `--redetect` ignores sidecars; `--save-annotations DIR` writes what
  was detected (never next to the inputs).
- **Matching rules:** shoe and handwriting points only pair with the
  same point type. For EBTS, a point without a type or of type
  `Other` may pair with any type, and two matched minutiae must keep
  their relative direction (`theta`) within
  `--align2d-minutia-angle-tol-deg` (default 20, 0 = off); the GUI
  Align dialog applies the same rules.
- **x3p alignments:** `[align3d] results` are distinct alignments,
  best first: after each one, every striation pair its transform
  explains is set aside and the search runs again, so later results
  are genuinely different placements rather than variants of the
  best. Each later one needs `min_matched` striations and at least
  `distinct_min_frac` (default 0.7) of the best's count. Alignments
  that rotate a land by more than `max_rotation_deg` (default 30;
  e.g. reversed end-for-end) are rejected. Results are reproducible:
  the same inputs give the same alignments for any `-j`. Every extra
  result costs another search, so keep `results` small for big runs.
  x3p `summary.csv` rows also carry `matched_2` (the second-best
  alignment's striations) and `ratio` = `matched / matched_2`: how far
  the best stands above any other placement; `0` and `inf` when no
  second one reached `distinct_min_frac`, empty for `none`/`error`.
  `results 2` is enough for it.
- **Parameters:** every value the Align / Detect dialogs expose is a
  flag (`--align2d-*`, `--detect2d-*`, `--striation-*`, `--align3d-*`)
  and a key in the INI file given with `-p`; `scpp params` prints them
  with their defaults. `-j N` sets the threads for every stage
  (default: all cores); results do not depend on it.
- **Output:** a summary per alignment on stdout; `--json` prints, and
  `-o FILE` writes, the full result, whose alignments use the GUI's
  *Save Alignment* format. `--save-matched DIR` writes each input's
  matched points/striations only (`matched_left.json` /
  `matched_right.json`, loadable in the GUI) and, for 2-D inputs, a
  PNG of each with the matched points numbered so left #k pairs with
  right #k.
- **Batch:** `--all-pairs` compares every pair in the folder (`-r` to
  recurse); `--pairs FILE` takes one `left,right` per line. The output
  folder gets `summary.csv` (one row per pair, best alignment; its
  `id` column names the pair's files), `inputs.csv` (`id,input`, which
  names each input's files), `pairs/cmp_<id>.json`, the `params.ini`
  used, and, with the `--save-annotations` / `--save-matched` flags,
  `annotations/in_<id>.json` and `matched/cmp_<id>_left.json` /
  `_right.json` (plus `.png` for 2-D). Input names only appear in the
  CSVs and inside the JSON, so long names are fine.
- **Cross:** `scpp cross DIR1 DIR2 --out-dir OUT` compares every input
  of `DIR1` with every input of `DIR2` (e.g. all lands of one bullet
  against all lands of another), with the same options and output
  folder as `batch`, plus `matrix.csv`: one row per `DIR1` input, one
  column per `DIR2` input, each cell the pair's matched count (empty
  for `error`/`timeout`). For x3p it also writes `matrix_ratio.csv`
  with each pair's `ratio` (below). Inputs are labelled `DIR/<file>`
  as given, and each is prepared once.
- **Failures:** an input or pair that fails (unreadable file, no
  annotations, an error or exception in a comparison) is recorded as
  `error` with its reason, and the batch moves on. `--timeout SEC`
  cancels any one detection or comparison that runs longer and records
  it as `timeout`. `summary.csv` is written as pairs finish;
  `scpp batch ... --resume` (or `scpp cross ... --resume`) with the same `--out-dir` and parameters
  keeps the finished `ok`/`none` pairs and runs the rest (including
  earlier `error`/`timeout` pairs).
- **Exit status:** 0 when every comparison ran (finding no match is
  not an error), 1 if an input or comparison failed or timed out, 2
  for bad arguments or parameters.

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
   at the full path of the library. `scpp --detect2d-model` finds the
   runtime the same way (next to `scpp`, or the environment variable).

The app targets the CPU execution provider by default and will opt into a
GPU provider (CUDA / DirectML / CoreML) automatically if the loaded
runtime build provides one, silently falling back to CPU otherwise. If
the installed runtime is older than the vendored header's API version,
the feature simply stays disabled.

[ort]: https://github.com/microsoft/onnxruntime
[ort-rel]: https://github.com/microsoft/onnxruntime/releases
