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
    };

    struct CompareArgs
    {
        std::string left, right;
        std::string leftAnn, rightAnn;
        bool json = false;    // full JSON on stdout
        std::string outFile;  // -o
    };

    struct BatchArgs
    {
        std::string dir;
        bool allPairs = false;
        std::string pairsFile;
        std::string outDir;
        bool recursive = false;
        bool saveAnnotations = false;  // -> <out>/annotations
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
}  // namespace shoecomp

#endif
