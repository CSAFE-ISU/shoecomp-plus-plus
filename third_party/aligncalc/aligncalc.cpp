#include "aligncalc.internal.h"
#include "alignWithRules.h"

namespace AlignCalc
{
    const char* const kNoPointMatch =
        "no consistent set of points found";

    bool getAlignment(const DoubleMatrixR& left_pts,
                      const DoubleMatrixR& right_pts,
                      const PointCols& cols, int labelCol,
                      const RTSParams& params, WorkerChannel& channel,
                      std::vector<MatchedPoints>& results)
    {
        LabelRules rules;
        rules.labelCol = labelCol;
        return getAlignmentWithRules(left_pts, right_pts, cols, rules,
                                     params, channel, results);
    }

} /* namespace AlignCalc */
