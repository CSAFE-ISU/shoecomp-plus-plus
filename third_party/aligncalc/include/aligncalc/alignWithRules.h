#ifndef ALIGNCALC_ALIGN_WITH_RULES_H
#define ALIGNCALC_ALIGN_WITH_RULES_H
#include "aligncalc.h"
#include "distinct.h"
#include "estimate.h"
#include "lineAnchor3d.h"
#include "pointAnchor2d.h"
#include "pointScaleAnchor.h"
#include "rigidLines3d.h"
#include "rigidPoints.h"
#include "similarityPoints.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <thread>

// alignPoints / alignLines3D with caller-defined Rules (see rules.h):
// admissible() filters nodes, edgeOk() / tripleOk() / tolScale() vet
// edges, acceptTransform() vets fitted alignments.
namespace AlignCalc
{
    namespace detail
    {
        // explains(): candidate (l, r) agrees with clique member k when
        // |s Rot (L_l - L_k) - (R_r - R_k)| <= this * epsilon * |R_r -
        // R_k|, and is explained when half the members agree.
        constexpr double kExplainTolFactor = 2.0;

        inline size_t resolveWorkers(size_t n)
        {
            if (n > 0) return n;
            const unsigned h = std::thread::hardware_concurrency();
            return h > 0 ? h : 1;
        }

        // Clique bounds: at least max(minMatched, 2) pairs, at most
        // the fewer of the two counts (and maxMatched).
        inline DistinctParams distinctParams(const CliqueOptions& c,
                                             Eigen::Index nL,
                                             Eigen::Index nR)
        {
            DistinctParams dp;
            dp.numWorkers = resolveWorkers(c.numWorkers);
            dp.numResults = c.results;
            dp.lowerBound = std::max<int32_t>(c.minMatched, 2);
            dp.upperBound = (int32_t)std::min(nL, nR);
            if (c.maxMatched > 0)
                dp.upperBound = std::min(dp.upperBound, c.maxMatched);
            dp.mode = c.distinct;
            dp.minFrac = c.distinctMinFrac;
            return dp;
        }

        template <class Rules>
        struct PointFitter
        {
            const DoubleMatrixR& left;
            const DoubleMatrixR& right;
            const PointCols& cols;
            const Rules& rules;
            double tol;
            bool withScale;
            double scaleRange;  // withScale: accepted scales
            std::vector<PointAlignment>& out;
            PointAlignment cur{};
            Eigen::MatrixXd sr{};  // scale * rotation

            bool fit(const std::vector<Correspondence>& clq)
            {
                cur = PointAlignment{};
                cur.pairs = clq;
                if (!estimatePointTransform(left, right, cols, clq,
                                            withScale, cur.transform))
                    return false;
                sr = cur.transform.scale * cur.transform.rotation;
                return true;
            }
            bool explains(Correspondence c) const
            {
                const int dim = cols.dim;
                double stack[12];
                std::vector<double> heap;
                if (dim > 3) heap.resize(4 * size_t(dim));
                double* const lc = dim > 3 ? heap.data() : stack;
                double* const rc = lc + dim;
                double* const dl = rc + dim;
                double* const dr = dl + dim;
                for (int d = 0; d < dim; ++d)
                {
                    lc[d] = left(c.left, cols.col(d));
                    rc[d] = right(c.right, cols.col(d));
                }
                const double tol2 = tol * tol;
                const int32_t n = (int32_t)cur.pairs.size();
                const int32_t need = (n + 1) / 2;
                int32_t agree = 0;
                for (int32_t k = 0; k < n; ++k)
                {
                    const Correspondence m = cur.pairs[k];
                    for (int d = 0; d < dim; ++d)
                    {
                        dl[d] = lc[d] - left(m.left, cols.col(d));
                        dr[d] = rc[d] - right(m.right, cols.col(d));
                    }
                    double e2 = 0.0, r2 = 0.0;
                    for (int i = 0; i < dim; ++i)
                    {
                        double e = sr(i, 0) * dl[0];
                        for (int j = 1; j < dim; ++j)
                            e += sr(i, j) * dl[j];
                        e -= dr[i];
                        e2 += e * e;
                        r2 += dr[i] * dr[i];
                    }
                    if (e2 <= tol2 * r2) ++agree;
                    if (agree >= need) return true;
                    if (agree + (n - k - 1) < need) return false;
                }
                return false;
            }
            bool accept() const
            {
                const double s = cur.transform.scale;
                if (withScale && scaleRange > 0.0 &&
                    (s < 1.0 / scaleRange || s > scaleRange))
                    return false;
                return rules.acceptTransform(cur.transform);
            }
            void keep(int32_t explained)
            {
                cur.explained = explained;
                out.push_back(std::move(cur));
            }
        };

        template <class Rules>
        struct LineFitter
        {
            const DoubleMatrixR& left;
            const DoubleMatrixR& right;
            const LineCols& cols;
            const Rules& rules;
            double angleTol, sepTol;
            std::vector<LineAlignment>& out;
            LineAlignment cur{};

            static Eigen::Vector3d mid(const DoubleMatrixR& m,
                                       const LineCols& c, int32_t i)
            {
                return {m(i, c.mid[0]), m(i, c.mid[1]), m(i, c.mid[2])};
            }
            static Eigen::Vector3d dir(const DoubleMatrixR& m,
                                       const LineCols& c, int32_t i)
            {
                return Eigen::Vector3d(m(i, c.dir[0]), m(i, c.dir[1]),
                                       m(i, c.dir[2]))
                    .normalized();
            }

            bool fit(const std::vector<Correspondence>& clq)
            {
                LineFit fit;
                if (!estimateRigidLines3D(left, right, cols, clq, fit))
                    return false;
                cur = LineAlignment{};
                cur.pairs = clq;
                cur.transform = fit.transform;
                cur.axis = fit.axis;
                cur.rmsPerp = fit.rmsPerp;
                cur.axialWeak = fit.axialWeak;
                return true;
            }
            // The transform maps left line l onto right line r: their
            // directions agree within angleTol (unless it is 0) and the
            // mapped left midpoint lies within sepTol of the right line
            // (the offset along the line is not checked; it is often
            // weakly fixed).
            bool explains(Correspondence c) const
            {
                const Eigen::Isometry3d& T = cur.transform;
                const Eigen::Vector3d dl =
                    T.linear() * dir(left, cols, c.left);
                const Eigen::Vector3d dr = dir(right, cols, c.right);
                const double cs = std::min(1.0, std::abs(dl.dot(dr)));
                if (angleTol > 0.0 && std::acos(cs) > angleTol)
                    return false;
                const Eigen::Vector3d q = T * mid(left, cols, c.left) -
                                          mid(right, cols, c.right);
                return (q - q.dot(dr) * dr).norm() <= sepTol;
            }
            bool accept() const
            {
                return rules.acceptTransform(cur.transform);
            }
            void keep(int32_t explained)
            {
                cur.explained = explained;
                out.push_back(std::move(cur));
            }
        };

        struct StageText
        {
            const char* invalid;
            const char* finding;
            const char* buildFailed;
            const char* noMatch;
        };

        // A cancelled call reports nothing, not the alignments found
        // before the cancel.
        inline void clearItems(PointResult& r) { r.alignments.clear(); }
        inline void clearItems(LineResult& r) { r.alignments.clear(); }
        inline void clearItems(MatchResult& r) { r.matches.clear(); }

        // Graph + distinct alignments for a valid |model|; fills
        // |result|'s status and message.
        template <class Model, class Fitter, class Result>
        void graphAndDistinct(const Model& model, const BuildParams& bp,
                              const DistinctParams& dp, Fitter& fitter,
                              Progress& progress, const StageText& text,
                              Result& result)
        {
            result.message.clear();
            if (dp.upperBound < dp.lowerBound)
            {
                result.status = Status::NoMatch;
                result.message = text.noMatch;
                return;
            }
            GraphRows graph;
            progress.report(0.25f, text.finding);
            if (!buildGraph(model, bp, progress, graph))
            {
                if (progress.cancelled())
                {
                    result.status = Status::Cancelled;
                    result.message = "cancelled";
                    return;
                }
                result.status = Status::Error;
                result.message = text.buildFailed;
                return;
            }
            progress.report(0.5f, "searching cliques...");
            switch (searchDistinct(graph, dp, fitter, progress))
            {
                case DistinctStatus::Ok:
                    result.status = Status::Ok;
                    return;
                case DistinctStatus::FitFailed:
                    result.status = Status::Error;
                    result.message = "transform fit failed";
                    return;
                case DistinctStatus::Cancelled:
                    result.status = Status::Cancelled;
                    result.message = "cancelled";
                    clearItems(result);
                    return;
                default:
                    result.status = Status::NoMatch;
                    result.message = text.noMatch;
                    return;
            }
        }

        constexpr StageText kPointText{
            "invalid point columns or parameters",
            "finding matchable points...", "could not fill graph",
            "no consistent set of points found"};
        constexpr StageText kLineText{
            "invalid line columns or tolerances",
            "finding matchable lines...",
            "could not build the match graph",
            "no consistent set of lines found"};

        // Graph + distinct cliques for a point |model|, with the
        // anchored search when options.anchorTol is set: PointAnchor2D
        // in 2-D, PointScaleAnchor for similarity in more dimensions.
        template <class Model, class Fitter, class Result>
        void runPointModel(const Model& model,
                           const DoubleMatrixR& left,
                           const DoubleMatrixR& right,
                           const PointCols& cols,
                           const PointOptions& options,
                           const CliqueOptions& clique, Fitter& fitter,
                           Progress& progress, Result& result)
        {
            if (!model.valid())
            {
                result.status = Status::Error;
                result.message = kPointText.invalid;
                return;
            }
            const bool similarity =
                options.mode == PointMode::Similarity;
            DistinctParams dp =
                distinctParams(clique, left.rows(), right.rows());
            // scale range 1: a rigid anchor, no reflection
            PointAnchor2D anchor(left, right, cols,
                                 similarity ? options.scaleRange : 1.0,
                                 options.anchorTol);
            std::unique_ptr<PointScaleAnchor> scaleAnchor;
            if (options.anchorTol > 0.0 && cols.dim == 2)
                dp.constraint = &anchor;
            else if (options.anchorTol > 0.0 && similarity)
            {
                scaleAnchor = std::make_unique<PointScaleAnchor>(
                    left, right, cols, options.scaleRange,
                    options.anchorTol);
                dp.constraint = scaleAnchor.get();
            }
            BuildParams bp;
            bp.numWorkers = dp.numWorkers;
            bp.minSupport = options.minSupport;
            graphAndDistinct(model, bp, dp, fitter, progress,
                             kPointText, result);
        }

        // runPointModel with the model options.mode asks for:
        // RigidPoints or SimilarityPoints.
        template <class Rules, class Fitter, class Result>
        void runPoints(const DoubleMatrixR& left,
                       const DoubleMatrixR& right,
                       const PointCols& cols, const Rules& rules,
                       const PointOptions& options,
                       const CliqueOptions& clique, Fitter& fitter,
                       Progress& progress, Result& result)
        {
            if (options.mode == PointMode::Rigid)
            {
                RigidPoints<Rules> model(left, right, cols,
                                         options.epsilon, rules);
                runPointModel(model, left, right, cols, options, clique,
                              fitter, progress, result);
                return;
            }
            if (cols.dim < 2)
            {
                result.status = Status::Error;
                result.message =
                    "similarity matching needs 2 or more dimensions";
                return;
            }
            SimilarityParams sp;
            sp.scaleRange = options.scaleRange;
            sp.epsilon = options.epsilon;
            sp.angleCol = cols.angle;
            sp.angleTolDeg = options.orientationTolDeg;
            SimilarityPoints<Rules> model(left, right, cols, sp, rules);
            runPointModel(model, left, right, cols, options, clique,
                          fitter, progress, result);
        }

        // searchDistinct "fitter" that fits nothing: every clique is
        // kept as found (Disjoint mode only; it explains nothing).
        struct MatchCollector
        {
            std::vector<PointMatch>& out;
            PointMatch cur{};

            bool fit(const std::vector<Correspondence>& clq)
            {
                cur.pairs = clq;
                return true;
            }
            bool explains(Correspondence) const { return false; }
            bool accept() const { return true; }
            void keep(int32_t) { out.push_back(std::move(cur)); }
        };
    }  // namespace detail

    // alignPoints with caller-defined Rules: rigid (RigidPoints:
    // edgeOk) or similarity (SimilarityPoints: tripleOk) matching.
    // cols.label is left to the Rules.
    template <class Rules>
    PointResult alignPointsWithRules(const DoubleMatrixR& left,
                                     const DoubleMatrixR& right,
                                     const PointCols& cols,
                                     const Rules& rules,
                                     const PointOptions& options,
                                     const CliqueOptions& clique,
                                     Progress& progress)
    {
        PointResult result;
        detail::PointFitter<Rules> fitter{
            left,
            right,
            cols,
            rules,
            detail::kExplainTolFactor * options.epsilon,
            options.mode == PointMode::Similarity,
            options.scaleRange,
            result.alignments};
        detail::runPoints(left, right, cols, rules, options, clique,
                          fitter, progress, result);
        return result;
    }

    // matchPoints with caller-defined Rules (acceptTransform is not
    // used: nothing is fitted). cols.label is left to the Rules.
    template <class Rules>
    MatchResult matchPointsWithRules(const DoubleMatrixR& left,
                                     const DoubleMatrixR& right,
                                     const PointCols& cols,
                                     const Rules& rules,
                                     const PointOptions& options,
                                     const CliqueOptions& clique,
                                     Progress& progress)
    {
        MatchResult result;
        CliqueOptions disjoint = clique;
        disjoint.distinct = DistinctMode::Disjoint;
        detail::MatchCollector collector{result.matches};
        detail::runPoints(left, right, cols, rules, options, disjoint,
                          collector, progress, result);
        return result;
    }

    // alignLines3D with caller-defined Rules. cols.label is left to
    // the Rules.
    template <class Rules>
    LineResult alignLines3DWithRules(const DoubleMatrixR& left,
                                     const DoubleMatrixR& right,
                                     const LineCols& cols,
                                     const Rules& rules,
                                     const LineParams& params,
                                     const CliqueOptions& clique,
                                     Progress& progress)
    {
        LineResult result;
        RigidLines3D<Rules> model(left, right, cols, params, rules);
        if (!model.valid())
        {
            result.message = detail::kLineText.invalid;
            return result;
        }
        DistinctParams dp =
            detail::distinctParams(clique, left.rows(), right.rows());
        const LineAnchor3D anchor(left, right, cols, params.sepTol,
                                  params.anchorTol, params.oriented);
        if (params.anchorTol > 0.0) dp.constraint = &anchor;
        BuildParams bp;
        bp.numWorkers = dp.numWorkers;
        detail::LineFitter<Rules> fitter{left,
                                         right,
                                         cols,
                                         rules,
                                         params.angleTol,
                                         params.sepTol,
                                         result.alignments};
        detail::graphAndDistinct(model, bp, dp, fitter, progress,
                                 detail::kLineText, result);
        return result;
    }
}  // namespace AlignCalc

#endif
