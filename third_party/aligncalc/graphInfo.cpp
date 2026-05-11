#include "graphInfo.internal.h"
#include <iostream>

namespace AlignCalc
{

    void GraphInfo::reset()
    {
        N = 0;
        nodemap.clear();
        edges.clear();
    }

    void GraphInfo::addNode(Node &v)
    {
        if (nodemap.find(v) == nodemap.end())
        {
            nodemap[v] = N;
            ++N;
        }
    }

    void GraphInfo::addEdge(Node &v, Node &w) { edges.insert({v, w}); }

} /* namespace AlignCalc */
