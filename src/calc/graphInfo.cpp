#include "calc/rts_align.h"
#include <iostream>

namespace shoecomp
{

    void GraphInfo::insert(Node& v)
    {
        if (nodemap.find(v) == nodemap.end())
        {
            nodemap[v] = N;
            ++N;
        }
    }

#define UPDATE_ADJMAT(t, i)     \
    {                           \
        update##i(t);           \
        insert(v1);             \
        insert(v2);             \
        insert(v3);             \
        edges.insert({v1, v2}); \
        edges.insert({v2, v3}); \
        edges.insert({v3, v1}); \
    }

    void GraphInfo::updateAll(const TaskInfo& t)
    {
        printf("updating with task: (%d, %d, %d) <-> (%d, %d, %d): ",
               t.i1, t.j1, t.k1, t.i2, t.j2, t.k2);
        for (int z = 0; z < 6; ++z) printf("%d ", t.check[z]);
        printf("\n");
        if (t.check[0]) UPDATE_ADJMAT(t, 0);
        if (t.check[1]) UPDATE_ADJMAT(t, 1);
        if (t.check[2]) UPDATE_ADJMAT(t, 2);
        if (t.check[3]) UPDATE_ADJMAT(t, 3);
        if (t.check[4]) UPDATE_ADJMAT(t, 4);
        if (t.check[5]) UPDATE_ADJMAT(t, 5);
    };

    void GraphInfo::update0(const TaskInfo& t)
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

    void GraphInfo::update1(const TaskInfo& t)
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

    void GraphInfo::update2(const TaskInfo& t)
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

    void GraphInfo::update3(const TaskInfo& t)
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

    void GraphInfo::update4(const TaskInfo& t)
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

    void GraphInfo::update5(const TaskInfo& t)
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

} /* namespace shoecomp */
