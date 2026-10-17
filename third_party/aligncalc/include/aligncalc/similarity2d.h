#ifndef ALIGNCALC_SIMILARITY2D_H
#define ALIGNCALC_SIMILARITY2D_H
#include "graphBuild.h"
#include "rules.h"
#include <memory>

namespace AlignCalc
{
    struct SimilarityParams
    {
        double scaleRange = 5.0;
        double epsilon = 0.005;
        // Oriented points (e.g. minutiae): with angleCol >= 0 and
        // angleTolDeg > 0, a triangle pair is skipped before the
        // similarity test unless the sorted circular gaps between its
        // three directions (degrees; rotation- and order-invariant)
        // agree within angleTolDeg. Points with a NaN angle disable
        // the check for their triangles. The exact per-pair direction
        // test is left to the Rules (tripleOk).
        int angleCol = -1;
        double angleTolDeg = 0.0;
    };

    namespace detail
    {
        // Triangle geometry for SimilarityPoints2D (independent of the
        // Rules, so it is compiled once in the library).
        class SimilarityIndex2D
        {
           public:
            ALIGNCALC_API SimilarityIndex2D(
                const DoubleMatrixR& left, const DoubleMatrixR& right,
                const PointCols& cols, const SimilarityParams& params);
            ALIGNCALC_API ~SimilarityIndex2D();
            SimilarityIndex2D(const SimilarityIndex2D&) = delete;
            SimilarityIndex2D& operator=(const SimilarityIndex2D&) =
                delete;

            ALIGNCALC_API bool valid() const;
            // appends every right triangle (and vertex ordering)
            // similar to the left triangle |l|
            ALIGNCALC_API void match(
                const std::array<int32_t, 3>& l,
                std::vector<TupleMatch<3>>& out) const;

           private:
            struct Impl;
            std::unique_ptr<Impl> impl_;
        };
    }  // namespace detail

    // Similarity (rotation, translation, scale) matching of 2-D points,
    // edges-first over triangles: a left and a right triangle match
    // when their side ratios agree within epsilon and the mean ratio
    // lies in [1/scaleRange, scaleRange]; every match joins its three
    // correspondences. Rules::tripleOk can veto a match.
    //
    // The model keeps references to |left| and |right|; they must
    // outlive it.
    template <class Rules = LabelRules>
    class SimilarityPoints2D
    {
       public:
        static constexpr int arity = 3;

        SimilarityPoints2D(const DoubleMatrixR& left,
                           const DoubleMatrixR& right,
                           const PointCols& cols,
                           const SimilarityParams& params,
                           Rules rules = Rules{})
            : L_(left), R_(right), index_(left, right, cols, params),
              rules_(std::move(rules)),
              nL_(index_.valid() ? int32_t(left.rows()) : 0),
              nR_(index_.valid() ? int32_t(right.rows()) : 0)
        {
        }
        SimilarityPoints2D(DoubleMatrixR&&, const DoubleMatrixR&,
                           const PointCols&, const SimilarityParams&,
                           Rules = Rules{}) = delete;
        SimilarityPoints2D(const DoubleMatrixR&, DoubleMatrixR&&,
                           const PointCols&, const SimilarityParams&,
                           Rules = Rules{}) = delete;
        SimilarityPoints2D(DoubleMatrixR&&, DoubleMatrixR&&,
                           const PointCols&, const SimilarityParams&,
                           Rules = Rules{}) = delete;

        bool valid() const { return index_.valid(); }
        int32_t leftCount() const { return nL_; }
        int32_t rightCount() const { return nR_; }
        bool admissible(int32_t l, int32_t r) const
        {
            return rules_.admissible(L_, l, R_, r);
        }
        void consistentWith(const std::array<int32_t, 3>& l,
                            std::vector<TupleMatch<3>>& out) const
        {
            const size_t first = out.size();
            index_.match(l, out);
            out.erase(std::remove_if(out.begin() + first, out.end(),
                                     [this](const TupleMatch<3>& m) {
                                         return !rules_.tripleOk(
                                             L_, m.left, R_, m.right);
                                     }),
                      out.end());
        }

       private:
        const DoubleMatrixR& L_;
        const DoubleMatrixR& R_;
        detail::SimilarityIndex2D index_;
        Rules rules_;
        int32_t nL_;
        int32_t nR_;
    };

    ALIGNCALC_EXTERN_TEMPLATE(
        class ALIGNCALC_API SimilarityPoints2D<LabelRules>)
    ALIGNCALC_EXTERN_TEMPLATE(ALIGNCALC_API bool buildEdgesFirst(
        const SimilarityPoints2D<LabelRules>&, const BuildParams&,
        Progress&, GraphRows&))
    ALIGNCALC_EXTERN_TEMPLATE(ALIGNCALC_API bool buildGraph(
        const SimilarityPoints2D<LabelRules>&, const BuildParams&,
        Progress&, GraphRows&))

}  // namespace AlignCalc

#endif
