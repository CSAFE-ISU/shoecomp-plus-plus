#include "aligncalc.internal.h"
#include "rigidPoints.h"
#include "similarity2d.h"

namespace AlignCalc
{
    template <class Model>
    static bool alignWith(const Model& model, const DoubleMatrixR& left,
                          const DoubleMatrixR& right,
                          const PointCols& cols,
                          const RTSParams& params,
                          WorkerChannel& channel,
                          std::vector<MatchedPoints>& results)
    {
        if (!model.valid())
        {
            channel.error("invalid point columns or parameters");
            return false;
        }
        BuildParams bp;
        bp.numWorkers = params.numWorkers;
        GraphRows graph;
        channel.report(0.25f, "finding matchable points...");
        if (!buildGraph(model, bp, channel, graph))
        {
            if (!channel.should_cancel())
                channel.error("could not fill graph");
            return false;
        }

        CliqueParams cp;
        cp.numWorkers = params.numWorkers;
        cp.numResults = params.numResults;
        cp.lowerBound = params.lowerBound;
        cp.upperBound = params.upperBound;
        std::vector<std::vector<Correspondence>> cliques;
        channel.report(0.5f, "searching cliques...");
        if (!searchCliques(graph, cp, channel, cliques))
        {
            if (!channel.should_cancel())
                channel.error("clique search failed");
            return false;
        }

        channel.report(0.75f, "fitting transforms...");
        for (const auto& clq : cliques)
        {
            results.push_back(MatchedPoints{});
            MatchedPoints& match = results.back();
            match.N = static_cast<int32_t>(clq.size());
            match.pairs = clq;
            match.left_pts.resize(match.N, cols.dim);
            match.right_pts.resize(match.N, cols.dim);
            for (int32_t i = 0; i < match.N; ++i)
                for (int d = 0; d < cols.dim; ++d)
                {
                    match.left_pts(i, d) =
                        left(clq[i].left, cols.pos[d]);
                    match.right_pts(i, d) =
                        right(clq[i].right, cols.pos[d]);
                }
        }
        return true;
    }

    bool getAlignment(const DoubleMatrixR& left_pts,
                      const DoubleMatrixR& right_pts,
                      const PointCols& cols, int labelCol,
                      const RTSParams& params, WorkerChannel& channel,
                      std::vector<MatchedPoints>& results)
    {
        LabelRules rules;
        rules.labelCol = labelCol;
        if (params.sameScale)
        {
            RigidPoints<> model(left_pts, right_pts, cols,
                                params.epsilon, rules);
            return alignWith(model, left_pts, right_pts, cols, params,
                             channel, results);
        }
        SimilarityParams sp;
        sp.scaleRange = params.scaleRange;
        sp.epsilon = params.epsilon;
        SimilarityPoints2D<> model(left_pts, right_pts, cols, sp,
                                   rules);
        return alignWith(model, left_pts, right_pts, cols, params,
                         channel, results);
    }

} /* namespace AlignCalc */
