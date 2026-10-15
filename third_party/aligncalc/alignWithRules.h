#ifndef ALIGNCALC_ALIGN_WITH_RULES_H
#define ALIGNCALC_ALIGN_WITH_RULES_H
#include "aligncalc.h"
#include "pointAnchor2d.h"
#include "rigidPoints.h"
#include "similarity2d.h"

namespace AlignCalc
{
    // Correspondence graph + maximum cliques for any 2-D point model;
    // the matched rows go to |results| (best first).
    template <class Model>
    bool alignWith(const Model& model, const DoubleMatrixR& left,
                   const DoubleMatrixR& right, const PointCols& cols,
                   const RTSParams& params, WorkerChannel& channel,
                   std::vector<MatchedPoints>& results,
                   const CliqueConstraint* constraint = nullptr)
    {
        if (!model.valid())
        {
            channel.error("invalid point columns or parameters");
            return false;
        }
        BuildParams bp;
        bp.numWorkers = params.numWorkers;
        bp.minSupport = params.minSupport;
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
        cp.constraint = constraint;
        std::vector<std::vector<Correspondence>> cliques;
        channel.report(0.5f, "searching cliques...");
        if (!searchCliques(graph, cp, channel, cliques))
        {
            if (!channel.should_cancel()) channel.error(kNoPointMatch);
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

    // getAlignment with caller-defined Rules: admissible() filters
    // nodes; edgeOk() (rigid, params.sameScale) or tripleOk()
    // (similarity) vet edges.
    template <class Rules>
    bool getAlignmentWithRules(const DoubleMatrixR& left_pts,
                               const DoubleMatrixR& right_pts,
                               const PointCols& cols,
                               const Rules& rules,
                               const RTSParams& params,
                               WorkerChannel& channel,
                               std::vector<MatchedPoints>& results)
    {
        if (params.sameScale)
        {
            RigidPoints<Rules> model(left_pts, right_pts, cols,
                                     params.epsilon, rules);
            if (!(params.anchorTol > 0.0))
                return alignWith(model, left_pts, right_pts, cols,
                                 params, channel, results);
            // scale range 1: a rigid anchor, no reflection
            PointAnchor2D anchor(left_pts, right_pts, cols, 1.0,
                                 params.anchorTol);
            return alignWith(model, left_pts, right_pts, cols, params,
                             channel, results, &anchor);
        }
        SimilarityParams sp;
        sp.scaleRange = params.scaleRange;
        sp.epsilon = params.epsilon;
        sp.angleCol = params.angleCol;
        sp.angleTolDeg = params.angleTolDeg;
        SimilarityPoints2D<Rules> model(left_pts, right_pts, cols, sp,
                                        rules);
        if (!(params.anchorTol > 0.0))
            return alignWith(model, left_pts, right_pts, cols, params,
                             channel, results);
        PointAnchor2D anchor(left_pts, right_pts, cols,
                             params.scaleRange, params.anchorTol);
        return alignWith(model, left_pts, right_pts, cols, params,
                         channel, results, &anchor);
    }
}  // namespace AlignCalc

#endif
