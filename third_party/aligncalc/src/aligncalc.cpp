#include "aligncalc/aligncalc.h"
#include "aligncalc/alignWithRules.h"

namespace AlignCalc
{
    PointResult alignPoints(const DoubleMatrixR& left,
                            const DoubleMatrixR& right,
                            const PointCols& cols,
                            const PointOptions& options,
                            const CliqueOptions& clique,
                            Progress* progress)
    {
        Progress none;
        LabelRules rules;
        rules.labelCol = cols.label;
        return alignPointsWithRules(left, right, cols, rules, options,
                                    clique,
                                    progress ? *progress : none);
    }

    MatchResult matchPoints(const DoubleMatrixR& left,
                            const DoubleMatrixR& right,
                            const PointCols& cols,
                            const PointOptions& options,
                            const CliqueOptions& clique,
                            Progress* progress)
    {
        Progress none;
        LabelRules rules;
        rules.labelCol = cols.label;
        return matchPointsWithRules(left, right, cols, rules, options,
                                    clique,
                                    progress ? *progress : none);
    }

    LineResult alignLines3D(const DoubleMatrixR& left,
                            const DoubleMatrixR& right,
                            const LineCols& cols,
                            const LineParams& params,
                            const CliqueOptions& clique,
                            Progress* progress)
    {
        Progress none;
        LabelRules rules;
        rules.labelCol = cols.label;
        return alignLines3DWithRules(left, right, cols, rules, params,
                                     clique,
                                     progress ? *progress : none);
    }

} /* namespace AlignCalc */
