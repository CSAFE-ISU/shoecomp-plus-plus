#include "rt/rtFill.internal.h"
#include <iostream>

namespace AlignCalc
{
    namespace RT
    {
#define UPDATE_ADJMAT(g, t, i) \
    {                          \
        update##i(t);          \
        g.addNode(v1);         \
        g.addNode(v2);         \
        g.addEdge(v1, v2);     \
        g.addEdge(v2, v1);     \
    }

        void Updater::updateAll(GraphInfo& g, const TaskInfo& t)
        {
            if (t.check)
            {
                UPDATE_ADJMAT(g, t, 0);
                UPDATE_ADJMAT(g, t, 1);
            }
        };

        void Updater::update0(const TaskInfo& t)
        {
            v1.ind1 = t.i1;
            v1.ind2 = t.i2;
            //
            v2.ind1 = t.j1;
            v2.ind2 = t.j2;
        };

        void Updater::update1(const TaskInfo& t)
        {
            v1.ind1 = t.i1;
            v1.ind2 = t.j2;
            //
            v2.ind1 = t.j1;
            v2.ind2 = t.i2;
        };

    }  // namespace RT

} /* namespace AlignCalc */
