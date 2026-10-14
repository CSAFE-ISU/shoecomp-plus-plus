#ifndef SHOECOMP_CALC_ALIGN
#define SHOECOMP_CALC_ALIGN

#include <vector>
#include "aligncalc/workerChannel.h"
#include "aligncalc/aligncalc.h"
#include "core/alignment.h"
#include "core/imageType.h"
#include "jtjson/json.h"

namespace shoecomp
{
    // Matched annotation rows (points[] indices) of one alignment.
    using MatchedRows = std::vector<AlignCalc::Correspondence>;

    // Which points may correspond. Shoe/HW: equal point types. EBTS:
    // an untyped or Other point matches any type, else equal types;
    // with minutiaAngleTolDeg > 0, two correspondences must also
    // agree on the relative direction of their minutiae (theta).
    struct PointMatchRules
    {
        ImageType type = ImageType::Shoe;
        double minutiaAngleTolDeg = 0.0;
    };

    // Automatic 2-D alignment of the annotation points
    // ({x, y, type} in points[]) of two images; same-type points
    // only. Communicates progress via |channel| and writes the
    // estimated transforms into |results| (best first). A
    // params.upperBound <= 0 means the smaller of the two point
    // counts. |matched| (optional) gets each result's matched rows.
    void runAutoAlign(const jt::Json& leftAnn, const jt::Json& rightAnn,
                      WorkerChannel& channel,
                      std::vector<AlignState>& results,
                      const AlignCalc::RTSParams& params,
                      const PointMatchRules& rules = {},
                      std::vector<MatchedRows>* matched = nullptr);
    void runRTSAlign(const jt::Json& leftAnn, const jt::Json& rightAnn,
                     WorkerChannel& channel,
                     std::vector<AlignState>& results,
                     const AlignCalc::RTSParams& params,
                     const PointMatchRules& rules = {},
                     std::vector<MatchedRows>* matched = nullptr);
}  // namespace shoecomp

#endif
