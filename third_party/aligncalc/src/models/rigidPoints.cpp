#include "aligncalc/rigidPoints.h"

namespace AlignCalc
{
    template class RigidPoints<LabelRules>;
    template bool buildNodesFirst(const RigidPoints<LabelRules>&,
                                  const BuildParams&, WorkerChannel&,
                                  GraphRows&);
    template bool buildGraph(const RigidPoints<LabelRules>&,
                             const BuildParams&, WorkerChannel&,
                             GraphRows&);
}  // namespace AlignCalc
