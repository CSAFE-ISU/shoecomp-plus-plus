#include <cstdio>
#include <filesystem>

#include "cli/cli.h"

namespace shoecomp
{
    int runCompare(CliRun& c, const CompareArgs& a)
    {
        Item left, right;
        std::string err;
        if (!parseItemSpec(c.type, a.left, left, err) ||
            !parseItemSpec(c.type, a.right, right, err))
        {
            std::fprintf(stderr, "scpp: %s\n", err.c_str());
            return kExitUsage;
        }
        if (!a.matchedDir.empty() && !c.run.saveAnnotationsDir.empty())
        {
            namespace fs = std::filesystem;
            std::error_code ec;
            if (fs::weakly_canonical(a.matchedDir, ec) ==
                fs::weakly_canonical(c.run.saveAnnotationsDir, ec))
            {
                std::fprintf(
                    stderr,
                    "scpp: --save-matched and --save-annotations "
                    "need different directories\n");
                return kExitUsage;
            }
        }
        left.annPath = a.leftAnn;
        right.annPath = a.rightAnn;

        for (Item* it : {&left, &right})
        {
            if (!prepareItem(*it, c.params, c.run))
            {
                std::fprintf(stderr, "scpp: %s: %s\n",
                             it->label.c_str(), it->error.c_str());
                return kExitError;
            }
        }

        const PairResult r = comparePair(left, right, c.params, c.run);
        const jt::Json doc = pairDocument(c.type, left, right, r);
        if (!a.outFile.empty() &&
            !writeTextFile(a.outFile, doc.toStringPretty() + "\n", err))
        {
            std::fprintf(stderr, "scpp: %s\n", err.c_str());
            return kExitError;
        }
        if (!a.matchedDir.empty() &&
            !saveMatched(a.matchedDir, "matched", "_left", "_right",
                         left, right, r, c.matchedResults, err))
        {
            std::fprintf(stderr, "scpp: %s\n", err.c_str());
            return kExitError;
        }
        if (a.json)
            std::printf("%s\n", doc.toStringPretty().c_str());
        else
            printSummary(stdout, left, right, r);
        return r.status == PairStatus::Error ||
                       r.status == PairStatus::Timeout
                   ? kExitError
                   : kExitOk;
    }
}  // namespace shoecomp
