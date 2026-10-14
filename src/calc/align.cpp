#include "calc/align.h"

namespace shoecomp
{
    void runAutoAlign(const jt::Json& leftAnn, const jt::Json& rightAnn,
                      WorkerChannel& channel,
                      std::vector<AlignState>& results,
                      const AlignCalc::RTSParams& params,
                      std::vector<MatchedRows>* matched)
    {
        results.clear();
        if (matched) matched->clear();
        runRTSAlign(leftAnn, rightAnn, channel, results, params,
                    matched);
    }
}  // namespace shoecomp
