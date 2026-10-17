#include "calc/align.h"

namespace shoecomp
{
    AlignCalc::Status runAutoAlign(const jt::Json& leftAnn,
                                   const jt::Json& rightAnn,
                                   WorkerChannel& channel,
                                   std::vector<AlignState>& results,
                                   const AlignCalc::RTSParams& params,
                                   const PointMatchRules& rules,
                                   std::vector<MatchedRows>* matched,
                                   std::string* message)
    {
        results.clear();
        if (matched) matched->clear();
        return runRTSAlign(leftAnn, rightAnn, channel, results, params,
                           rules, matched, message);
    }
}  // namespace shoecomp
