#ifndef SHOECOMP_CORE_PIPELINE_H
#define SHOECOMP_CORE_PIPELINE_H

#include <functional>
#include <string>
#include <vector>

#include "calc/align.h"
#include "calc/align3d.h"
#include "core/alignment.h"
#include "core/imageType.h"
#include "core/params.h"
#include "jtjson/json.h"

// Headless prepare + compare: what the GUI's Detect / Align dialogs do,
// without a window. Used by the scpp CLI.
namespace shoecomp
{
    struct RunOptions
    {
        int threads = 0;        // 0 = hardware concurrency
        bool redetect = false;  // ignore sidecars, always detect
        // Per step (one input's detection, one pair's alignment); on
        // expiry the step is cancelled. 0 = no limit.
        double timeoutSec = 0.0;
        // Detection output directory (full detected annotations per
        // input, <stem>.json); empty = don't save.
        std::string saveAnnotationsDir;
        std::function<void(const std::string&)> log;  // may be empty
    };

    // One comparable input: a file, or one record of an EBTS file.
    struct Item
    {
        ImageType type = ImageType::Shoe;
        std::string path;
        int record = -1;  // EBTS image record index, -1 = whole file
        std::string label;
        std::string annPath;  // explicit annotations; empty = sidecar

        // Filled by prepareItem.
        bool prepared = false;
        std::string error;      // non-empty: preparation failed
        bool timedOut = false;  // preparation hit the timeout
        std::string annSource;  // where the annotations came from
        jt::Json annotations;
        size_t count = 0;               // 2-D points or 3-D striations
        AlignCalc::DoubleMatrixR rows;  // 3-D striation rows
        std::vector<int> peakIdx;       // 3-D row -> peaks[]
    };

    enum class PairStatus
    {
        Ok,       // at least one alignment
        NoMatch,  // ran (or could not run), nothing found
        Timeout,  // cancelled after RunOptions::timeoutSec
        Error
    };

    struct PairResult
    {
        PairStatus status = PairStatus::Error;
        std::string message;  // error text, or why nothing matched
        double timeMs = 0.0;
        jt::Json alignments;  // Save Alignment objects, best first
        // Matched rows per alignment: 2-D points[] indices, 3-D
        // peaks[] indices (left, right).
        std::vector<std::vector<std::pair<int, int>>> matched;
        std::vector<AlignState> align2d;
        std::vector<StriationAlignment> align3d;
    };

    // ok / none / timeout / error
    const char* pairStatusName(PairStatus s);
    int resolveThreads(int threads);

    // "a.png", or "file.ebts@N" for EBTS record N (0-based). Sets
    // path/record/label (label = file name, plus @N).
    bool parseItemSpec(ImageType type, const std::string& spec,
                       Item& out, std::string& err);

    // The items a file contributes to a batch: one per EBTS image
    // record for EBTS transaction files, otherwise one.
    bool itemsForFile(ImageType type, const std::string& path,
                      std::vector<Item>& out, std::string& err);

    // "<dir>/<stem>.json" (EBTS records: "<stem>@N.json").
    std::string sidecarPath(const Item& it);
    std::string annotationFileName(const Item& it);

    // Resolve the item's annotations: annPath, else the sidecar
    // (unless redetect), else EBTS Type-9 (2-D EBTS records), else
    // detection (ONNX model for 2-D, striation detection for x3p).
    // On failure returns false with it.error set.
    bool prepareItem(Item& it, const Params& params,
                     const RunOptions& opts);

    // Align two prepared items of the same type.
    PairResult comparePair(const Item& left, const Item& right,
                           const Params& params,
                           const RunOptions& opts);

    // RGBA pixels of a 2-D item (file or EBTS record).
    bool loadItemPixels(const Item& it,
                        std::vector<unsigned char>& rgba, int& w,
                        int& h, std::string& err);
}  // namespace shoecomp

#endif
