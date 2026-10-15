#ifndef SHOECOMP_CLI_CLI_H
#define SHOECOMP_CLI_CLI_H

#include <cstdio>
#include <string>
#include <vector>

#include "core/params.h"
#include "core/pipeline.h"
#include "jtjson/json.h"

namespace shoecomp
{
    enum ExitCode
    {
        kExitOk = 0,
        kExitError = 1,  // a comparison or an input failed
        kExitUsage = 2   // bad arguments / params
    };

    // Options shared by the single compare and batch modes.
    struct CliRun
    {
        ImageType type = ImageType::Shoe;
        Params params;
        RunOptions run;
        bool quiet = false;
        int matchedResults = 1;  // alignments written by --save-matched
    };

    struct CompareArgs
    {
        std::string left, right;
        std::string leftAnn, rightAnn;
        bool json = false;       // full JSON on stdout
        std::string outFile;     // -o
        std::string matchedDir;  // --save-matched
    };

    // `scpp batch` (pairs within DIR) and `scpp cross` (every DIR
    // input against every DIR2 input, plus matrix.csv).
    struct BatchArgs
    {
        std::string dir;
        std::string dir2;  // cross: the column inputs
        bool cross = false;
        bool allPairs = false;
        std::string pairsFile;
        std::string outDir;
        bool recursive = false;
        bool saveAnnotations = false;  // -> <out>/annotations
        bool saveMatched = false;      // -> <out>/matched
        bool resume = false;  // keep finished ok/none pairs in <out>
    };

    int runCompare(CliRun& c, const CompareArgs& a);
    int runBatch(CliRun& c, const BatchArgs& a);

    // Pair document: {type, left, right, status, message?, timeMs,
    // leftCount, rightCount, leftSource, rightSource, alignments}.
    jt::Json pairDocument(ImageType type, const Item& left,
                          const Item& right, const PairResult& r);
    void printSummary(std::FILE* f, const Item& left, const Item& right,
                      const PairResult& r);
    // RFC 4180 quoting when needed.
    std::string csvField(const std::string& s);
    // summary.csv; |id| is the pair's number in the batch, which also
    // names its files (pairs/cmp_<id>.json, matched/cmp_<id>_*).
    // Inputs are numbered the same way in inputs.csv (id,input), which
    // names annotations/in_<id>.json.
    std::string csvHeader(ImageType type);
    std::string csvRow(size_t id, const Item& left, const Item& right,
                       const PairResult& r);
    bool writeTextFile(const std::string& path, const std::string& text,
                       std::string& err);

    // File-name-safe form of an item label.
    std::string fileSafe(const std::string& label);

    // Matched-only output for the first |maxResults| alignments:
    // <base>[_rK]<leftTag>.json / <base>[_rK]<rightTag>.json (points
    // / peaks[] in pair order) and, for 2-D types, a .png of each with
    // numbered markers. Kept apart from detection output.
    bool saveMatched(const std::string& dir, const std::string& base,
                     const std::string& leftTag,
                     const std::string& rightTag, const Item& left,
                     const Item& right, const PairResult& r,
                     int maxResults, std::string& err);

    struct Marker
    {
        double x, y;  // image pixels, top-left origin
        PointColor color;
        int number;  // drawn next to the marker
    };
    // Filled, outlined circles with white-boxed numbers, sized to the
    // image.
    void drawMarkers(std::vector<unsigned char>& rgba, int w, int h,
                     const std::vector<Marker>& markers);
}  // namespace shoecomp

#endif
