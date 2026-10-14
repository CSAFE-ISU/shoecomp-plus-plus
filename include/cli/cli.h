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

    struct BatchArgs
    {
        std::string dir;
        bool allPairs = false;
        std::string pairsFile;
        std::string outDir;
        bool recursive = false;
        bool saveAnnotations = false;  // -> <out>/annotations
        bool saveMatched = false;      // -> <out>/matched
    };

    int runCompare(CliRun& c, const CompareArgs& a);
    int runBatch(CliRun& c, const BatchArgs& a);

    // Pair document: {type, left, right, status, message?, timeMs,
    // leftCount, rightCount, leftSource, rightSource, alignments}.
    jt::Json pairDocument(ImageType type, const Item& left,
                          const Item& right, const PairResult& r);
    void printSummary(std::FILE* f, const Item& left, const Item& right,
                      const PairResult& r);
    std::string csvHeader(ImageType type);
    std::string csvRow(const Item& left, const Item& right,
                       const PairResult& r);
    bool writeTextFile(const std::string& path, const std::string& text,
                       std::string& err);

    // File-name-safe form of an item label.
    std::string fileSafe(const std::string& label);

    // Matched-only output for the first |maxResults| alignments:
    // <A>__<B>[_rK].{left,right}.matched.json (points / peaks[] in
    // pair order) and, for 2-D types, .matched.png with numbered
    // markers. Kept apart from detection output.
    bool saveMatched(const std::string& dir, const Item& left,
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
