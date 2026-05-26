#include "aligncalc.internal.h"
#include "clq/clqmtch.h"
#include <iostream>

namespace AlignCalc
{
    bool getAlignment(const DoubleMatrixR& left_pts,
                      const DoubleMatrixR& right_pts,
                      const RTSParams& params, WorkerChannel& channel,
                      std::vector<MatchedPoints>& results)
    {
        GraphInfo g_info;
        clqmtch::StackDFS S(params.numWorkers, params.numResults,
                            params.lowerBound, params.upperBound);
        clqmtch::Graph G;
        std::vector<std::unordered_set<int32_t>> cliques;
        g_info.reset();
        //
        if (!RTS::fillGraph(left_pts, right_pts, params, channel,
                            g_info))
        {
            channel.error("could not fill graph");
            return false;
        }

        if (!G.init(channel, g_info) ||
            !S.processGraph(channel, G, cliques))
        {
            channel.error("clique search failed");
            return false;
        }

        channel.report(0.75f, "fitting transforms...");
        for (const auto& clq : cliques)
        {
            if (clq.size() < params.lowerBound) continue;
            results.push_back(MatchedPoints{});
            MatchedPoints& match = results.back();
            match.N = clq.size();
            match.left_pts.resize(match.N, 3);
            match.right_pts.resize(match.N, 3);
            //
            int i = 0;
            for (const i32& c : clq)
            {
                const clqmtch::Vertex& v = G.vertices[c];
                match.left_pts.row(i) = left_pts.row(v.ind1);
                match.right_pts.row(i) = right_pts.row(v.ind2);
                std::cout << c << ": " << match.left_pts.row(i)
                          << "<--->" << match.right_pts.row(i) << "\n";
                ++i;
            }
            printf("\n\n");
        }
        printf("%ld matched results\n", results.size());
        return true;
    }

} /* namespace AlignCalc */
