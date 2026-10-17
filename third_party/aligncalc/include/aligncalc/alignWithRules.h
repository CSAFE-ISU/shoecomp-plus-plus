#ifndef ALIGNCALC_ALIGN_WITH_RULES_H
#define ALIGNCALC_ALIGN_WITH_RULES_H
#include "aligncalc.h"
#include "distinct.h"
#include "pointAnchor2d.h"
#include "rigidPoints.h"
#include "similarity2d.h"

namespace AlignCalc
{
    namespace detail
    {
        // explains(): candidate (l, r) agrees with clique member k when
        // |s Rot (L_l - L_k) - (R_r - R_k)| <= this * epsilon * |R_r -
        // R_k|, and is explained when half the members agree.
        constexpr double kExplainTolFactor = 2.0;

        struct PointFitter
        {
            const DoubleMatrixR& left;
            const DoubleMatrixR& right;
            const PointCols& cols;
            double tol;
            bool sameScale;
            std::vector<MatchedPoints>& out;
            MatchedPoints cur{};
            double a = 1.0, b = 0.0;  // s cos(rot), s sin(rot)

            bool fit(const std::vector<Correspondence>& clq)
            {
                cur = MatchedPoints{};
                cur.N = static_cast<int32_t>(clq.size());
                cur.pairs = clq;
                cur.left_pts.resize(cur.N, cols.dim);
                cur.right_pts.resize(cur.N, cols.dim);
                for (int32_t i = 0; i < cur.N; ++i)
                    for (int d = 0; d < cols.dim; ++d)
                    {
                        cur.left_pts(i, d) =
                            left(clq[i].left, cols.pos[d]);
                        cur.right_pts(i, d) =
                            right(clq[i].right, cols.pos[d]);
                    }
                RTSTransform t;
                t.estimate(cur, false);
                const double s = sameScale ? 1.0 : t.scale;
                if (!std::isfinite(s) || !std::isfinite(t.rotation))
                    return false;
                a = s * std::cos(t.rotation);
                b = s * std::sin(t.rotation);
                return true;
            }
            bool explains(Correspondence c) const
            {
                const double lx = left(c.left, cols.pos[0]);
                const double ly = left(c.left, cols.pos[1]);
                const double rx = right(c.right, cols.pos[0]);
                const double ry = right(c.right, cols.pos[1]);
                const double tol2 = tol * tol;
                const int32_t need = (cur.N + 1) / 2;
                int32_t agree = 0;
                for (int32_t k = 0; k < cur.N; ++k)
                {
                    const double dlx = lx - cur.left_pts(k, 0);
                    const double dly = ly - cur.left_pts(k, 1);
                    const double drx = rx - cur.right_pts(k, 0);
                    const double dry = ry - cur.right_pts(k, 1);
                    const double ex = a * dlx - b * dly - drx;
                    const double ey = b * dlx + a * dly - dry;
                    if (ex * ex + ey * ey <=
                        tol2 * (drx * drx + dry * dry))
                        ++agree;
                    if (agree >= need) return true;
                    if (agree + (cur.N - k - 1) < need) return false;
                }
                return false;
            }
            bool accept() const { return true; }
            void keep(int32_t explained)
            {
                cur.explained = explained;
                out.push_back(std::move(cur));
            }
        };
    }  // namespace detail

    // Correspondence graph + distinct alignments (searchDistinct) for
    // any 2-D point model; the matched rows go to |results| (best
    // first). Anything but Status::Ok sets |message|.
    template <class Model>
    Status alignWith(const Model& model, const DoubleMatrixR& left,
                     const DoubleMatrixR& right, const PointCols& cols,
                     const RTSParams& params, Progress& progress,
                     std::vector<MatchedPoints>& results,
                     std::string& message,
                     const CliqueConstraint* constraint = nullptr)
    {
        message.clear();
        if (!model.valid())
        {
            message = "invalid point columns or parameters";
            return Status::Error;
        }
        BuildParams bp;
        bp.numWorkers = params.numWorkers;
        bp.minSupport = params.minSupport;
        GraphRows graph;
        progress.report(0.25f, "finding matchable points...");
        if (!buildGraph(model, bp, progress, graph))
        {
            if (progress.cancelled())
            {
                message = "cancelled";
                return Status::Cancelled;
            }
            message = "could not fill graph";
            return Status::Error;
        }

        DistinctParams dp;
        dp.numWorkers = params.numWorkers;
        dp.numResults = params.numResults;
        dp.lowerBound = params.lowerBound;
        dp.upperBound = params.upperBound;
        dp.constraint = constraint;
        dp.mode = params.distinct;
        detail::PointFitter fitter{
            left,
            right,
            cols,
            detail::kExplainTolFactor * params.epsilon,
            params.sameScale,
            results};
        progress.report(0.5f, "searching cliques...");
        switch (searchDistinct(graph, dp, fitter, progress))
        {
            case DistinctStatus::Ok:
                return Status::Ok;
            case DistinctStatus::FitFailed:
                message = "transform fit failed";
                return Status::Error;
            case DistinctStatus::Cancelled:
                message = "cancelled";
                return Status::Cancelled;
            default:
                message = "no consistent set of points found";
                return Status::NoMatch;
        }
    }

    // getAlignment with caller-defined Rules: admissible() filters
    // nodes; edgeOk() (rigid, params.sameScale) or tripleOk()
    // (similarity) vet edges.
    template <class Rules>
    Status getAlignmentWithRules(
        const DoubleMatrixR& left_pts, const DoubleMatrixR& right_pts,
        const PointCols& cols, const Rules& rules,
        const RTSParams& params, Progress& progress,
        std::vector<MatchedPoints>& results, std::string& message)
    {
        if (params.sameScale)
        {
            RigidPoints<Rules> model(left_pts, right_pts, cols,
                                     params.epsilon, rules);
            if (!(params.anchorTol > 0.0))
                return alignWith(model, left_pts, right_pts, cols,
                                 params, progress, results, message);
            // scale range 1: a rigid anchor, no reflection
            PointAnchor2D anchor(left_pts, right_pts, cols, 1.0,
                                 params.anchorTol);
            return alignWith(model, left_pts, right_pts, cols, params,
                             progress, results, message, &anchor);
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
                             progress, results, message);
        PointAnchor2D anchor(left_pts, right_pts, cols,
                             params.scaleRange, params.anchorTol);
        return alignWith(model, left_pts, right_pts, cols, params,
                         progress, results, message, &anchor);
    }
}  // namespace AlignCalc

#endif
