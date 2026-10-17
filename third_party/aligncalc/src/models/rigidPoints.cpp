#include "aligncalc/rigidPoints.h"

namespace AlignCalc
{
    template class ALIGNCALC_INSTANTIATION_API RigidPoints<LabelRules>;
    template ALIGNCALC_INSTANTIATION_API bool buildNodesFirst(
        const RigidPoints<LabelRules>&, const BuildParams&, Progress&,
        GraphRows&);
    template ALIGNCALC_INSTANTIATION_API bool buildGraph(
        const RigidPoints<LabelRules>&, const BuildParams&, Progress&,
        GraphRows&);
}  // namespace AlignCalc
