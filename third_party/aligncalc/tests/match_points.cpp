// matchPoints (no transform fit): on the seeded 2-D datasets its
// matches are alignPoints' alignments in Disjoint mode (same pairs,
// same order) unless alignPoints set a fit aside (similarity scale
// outside scaleRange); results do not depend on the worker count; two
// copies give two point-disjoint matches; errors, no match and
// cancellation report like alignPoints.
#include "aligncalc/aligncalc.h"
#include "check.h"
#include "datasets.h"
#include <cmath>
#include <random>
using namespace AlignCalc;

namespace
{
    bool samePairs(const MatchResult& m, const PointResult& a)
    {
        if (m.status != a.status ||
            m.matches.size() != a.alignments.size())
            return false;
        for (size_t i = 0; i < m.matches.size(); ++i)
            if (m.matches[i].pairs != a.alignments[i].pairs)
                return false;
        return true;
    }

    bool sameMatches(const MatchResult& a, const MatchResult& b)
    {
        if (a.status != b.status ||
            a.matches.size() != b.matches.size())
            return false;
        for (size_t i = 0; i < a.matches.size(); ++i)
            if (a.matches[i].pairs != b.matches[i].pairs) return false;
        return true;
    }

    // any fitted scale outside the range: alignPoints sets it aside
    bool anyOutOfRange(const DoubleMatrixR& L, const DoubleMatrixR& R,
                       const MatchResult& m, double range)
    {
        for (const PointMatch& x : m.matches)
        {
            PointTransform t;
            if (!estimatePointTransform(L, R, PointCols{}, x.pairs,
                                        true, t) ||
                t.scale < 1 / range || t.scale > range)
                return true;
        }
        return false;
    }

    class CancelNow : public Progress
    {
       public:
        bool cancelled() const override { return true; }
    };
}  // namespace

int main()
{
    // datasets.h: matchPoints == alignPoints(Disjoint)
    int same = 0, compared = 0, skipped = 0;
    for (const Dataset& d : datasets())
    {
        PointOptions po;
        po.mode =
            d.sameScale ? PointMode::Rigid : PointMode::Similarity;
        CliqueOptions co;
        co.maxMatched = 500;
        co.results = 3;
        co.distinct = DistinctMode::Disjoint;
        co.numWorkers = 1;
        const MatchResult m =
            matchPoints(d.L, d.R, PointCols{}, po, co);
        co.distinct = DistinctMode::Explained;  // ignored
        CHECK(
            sameMatches(m, matchPoints(d.L, d.R, PointCols{}, po, co)));
        co.numWorkers = 8;
        CHECK(
            sameMatches(m, matchPoints(d.L, d.R, PointCols{}, po, co)));
        co.distinct = DistinctMode::Disjoint;
        co.numWorkers = 1;
        const PointResult a =
            alignPoints(d.L, d.R, PointCols{}, po, co);
        if (!d.sameScale && anyOutOfRange(d.L, d.R, m, po.scaleRange))
        {
            ++skipped;
            continue;
        }
        ++compared;
        same += samePairs(m, a);
        for (const PointMatch& x : m.matches)
            for (size_t i = 0; i < x.pairs.size(); ++i)
                for (size_t j = 0; j < i; ++j)
                    CHECK(x.pairs[i].left != x.pairs[j].left &&
                          x.pairs[i].right != x.pairs[j].right);
    }
    printf(
        "matchPoints == alignPoints(disjoint) on %d/%d datasets (%d "
        "skipped: a fit out of the scale range)\n",
        same, compared, skipped);
    CHECK(compared >= 60);
    CHECK(same == compared);

    // two copies under different rigid transforms, from different
    // left points
    {
        std::mt19937 rng(3);
        std::uniform_real_distribution<double> U(0, 1000);
        DoubleMatrixR L(46, 2), R(36, 2);
        for (int i = 0; i < 46; ++i) L(i, 0) = U(rng), L(i, 1) = U(rng);
        const double th[2] = {0.4, -2.1},
                     t[2][2] = {{1200, -300}, {-800, 1500}};
        for (int i = 0; i < 36; ++i)
        {
            const int k = i < 20 ? 0 : 1;
            const double c = std::cos(th[k]), s = std::sin(th[k]);
            R(i, 0) = c * L(i, 0) - s * L(i, 1) + t[k][0];
            R(i, 1) = s * L(i, 0) + c * L(i, 1) + t[k][1];
        }
        CliqueOptions co;
        co.results = 3;
        const MatchResult m = matchPoints(L, R, PointCols{}, {}, co);
        CHECKF(m.status == Status::Ok && m.matches.size() >= 2,
               "two copies: status %d, %zu matches", int(m.status),
               m.matches.size());
        if (m.matches.size() >= 2)
            for (int k = 0; k < 2; ++k)
            {
                const auto& p = m.matches[k].pairs;
                bool ok = p.size() == (k ? 16u : 20u);
                for (const Correspondence& c : p)
                    ok = ok && c.left == c.right && (c.left < 20) == !k;
                CHECKF(ok, "two copies: match %d is not copy %d", k, k);
            }
    }

    // errors, no match, cancellation
    {
        DoubleMatrixR L3 = DoubleMatrixR::Random(20, 3), R3 = L3;
        PointCols c3;
        c3.dim = 3;
        PointOptions sim;
        sim.mode = PointMode::Similarity;
        PointCols c1;  // similarity needs 2 or more dimensions
        c1.dim = 1;
        const MatchResult e = matchPoints(L3, R3, c1, sim, {});
        CHECK(e.status == Status::Error && !e.message.empty() &&
              e.matches.empty());
        // rigid 3-D: R3 = L3 matches row for row
        const MatchResult r = matchPoints(L3, R3, c3, {}, {});
        CHECK(r.status == Status::Ok &&
              r.matches[0].pairs.size() == 20);

        std::mt19937 rng(4);
        std::uniform_real_distribution<double> U(0, 1000);
        DoubleMatrixR A(20, 2), B(20, 2);
        for (int i = 0; i < 20; ++i)
            A(i, 0) = U(rng), A(i, 1) = U(rng), B(i, 0) = U(rng),
                 B(i, 1) = U(rng);
        CliqueOptions many;
        many.minMatched = 15;
        const MatchResult n = matchPoints(A, B, PointCols{}, {}, many);
        CHECK(n.status == Status::NoMatch && n.matches.empty() &&
              !n.message.empty());
        CancelNow cancel;
        const MatchResult x = matchPoints(L3, R3, c3, {}, {}, &cancel);
        CHECK(x.status == Status::Cancelled && x.matches.empty());
    }
    return checkResult();
}
