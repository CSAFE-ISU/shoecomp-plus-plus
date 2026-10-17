#include "calc/align.h"

namespace shoecomp
{
    AlignCalc::Status runAutoAlign(
        const jt::Json& leftAnn, const jt::Json& rightAnn,
        WorkerChannel& channel, std::vector<AlignState>& results,
        const AlignCalc::PointOptions& options,
        const AlignCalc::CliqueOptions& clique,
        const PointMatchRules& rules, std::vector<MatchedRows>* matched,
        std::string* message)
    {
        results.clear();
        if (matched) matched->clear();
        return runRTSAlign(leftAnn, rightAnn, channel, results, options,
                           clique, rules, matched, message);
    }
}  // namespace shoecomp
