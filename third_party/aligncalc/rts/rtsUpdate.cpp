#include "rts/rtsFill.internal.h"
#include <iostream>

namespace AlignCalc
{
    namespace RTS
    {
#define UPDATE_ADJMAT(g, t, i) \
    {                          \
        update##i(t);          \
        g.addNode(v1);         \
        g.addNode(v2);         \
        g.addNode(v3);         \
        g.addEdge(v1, v2);     \
        g.addEdge(v1, v3);     \
        g.addEdge(v2, v1);     \
        g.addEdge(v2, v3);     \
        g.addEdge(v3, v2);     \
        g.addEdge(v3, v1);     \
    }

        void Updater::updateAll(GraphInfo& g, const TaskInfo& t)
        {
            if (t.check[0]) UPDATE_ADJMAT(g, t, 0);
            if (t.check[1]) UPDATE_ADJMAT(g, t, 1);
            if (t.check[2]) UPDATE_ADJMAT(g, t, 2);
            if (t.check[3]) UPDATE_ADJMAT(g, t, 3);
            if (t.check[4]) UPDATE_ADJMAT(g, t, 4);
            if (t.check[5]) UPDATE_ADJMAT(g, t, 5);
        };

        void Updater::update0(const TaskInfo& t)
        {
            v1.ind1 = t.i1;
            v1.ind2 = t.i2;
            //
            v2.ind1 = t.j1;
            v2.ind2 = t.j2;
            //
            v3.ind1 = t.k1;
            v3.ind2 = t.k2;
        };

        void Updater::update1(const TaskInfo& t)
        {
            v1.ind1 = t.i1;
            v1.ind2 = t.i2;
            //
            v2.ind1 = t.j1;
            v2.ind2 = t.k2;
            //
            v3.ind1 = t.k1;
            v3.ind2 = t.j2;
        };

        void Updater::update2(const TaskInfo& t)
        {
            v1.ind1 = t.i1;
            v1.ind2 = t.j2;
            //
            v2.ind1 = t.j1;
            v2.ind2 = t.i2;
            //
            v3.ind1 = t.k1;
            v3.ind2 = t.k2;
        };

        void Updater::update3(const TaskInfo& t)
        {
            v1.ind1 = t.i1;
            v1.ind2 = t.j2;
            //
            v2.ind1 = t.j1;
            v2.ind2 = t.k2;
            //
            v3.ind1 = t.k1;
            v3.ind2 = t.i2;
        };

        void Updater::update4(const TaskInfo& t)
        {
            v1.ind1 = t.i1;
            v1.ind2 = t.k2;
            //
            v2.ind1 = t.j1;
            v2.ind2 = t.j2;
            //
            v3.ind1 = t.k1;
            v3.ind2 = t.i2;
        };

        void Updater::update5(const TaskInfo& t)
        {
            v1.ind1 = t.i1;
            v1.ind2 = t.k2;
            //
            v2.ind1 = t.j1;
            v2.ind2 = t.i2;
            //
            v3.ind1 = t.k1;
            v3.ind2 = t.j2;
        };

    } /* namespace RTS */

} /* namespace AlignCalc */
