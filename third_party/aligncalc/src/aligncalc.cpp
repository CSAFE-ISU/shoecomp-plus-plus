#include "aligncalc/aligncalc.h"
#include "aligncalc/alignWithRules.h"

namespace AlignCalc
{
    Status getAlignment(const DoubleMatrixR& left_pts,
                        const DoubleMatrixR& right_pts,
                        const PointCols& cols, int labelCol,
                        const RTSParams& params, Progress& progress,
                        std::vector<MatchedPoints>& results,
                        std::string& message)
    {
        LabelRules rules;
        rules.labelCol = labelCol;
        return getAlignmentWithRules(left_pts, right_pts, cols, rules,
                                     params, progress, results,
                                     message);
    }

} /* namespace AlignCalc */
