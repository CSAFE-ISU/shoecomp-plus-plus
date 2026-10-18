// Distinct alignments: right holds two rigid copies of left points
// under different transforms (plus outliers). The first two results
// must be the two copies, larger first, each with its own transform.
//  - separate: the copies come from different left points; both
//    DistinctMode::Explained and Disjoint find both.
//  - shared: both copies come from the same left points; Explained
//    finds both, Disjoint (which sets aside every pair sharing a
//    point with the first result) cannot find the second.
// Also: asking for fewer results keeps the first ones unchanged.
#include "aligncalc/aligncalc.h"
#include "check.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <set>
using namespace AlignCalc;

namespace
{
    struct Rigid2D
    {
        double angle, tx, ty;
    };

    struct Data
    {
        DoubleMatrixR L, R;
        std::set<std::pair<int, int>> truth[2];  // per copy
    };

    // copy k maps left rows [first[k], first[k] + size[k]) by T[k]
    Data make(unsigned seed, const int first[2], const int size[2],
              int nL, int outR, const Rigid2D T[2])
    {
        std::mt19937 rng(seed);
        std::uniform_real_distribution<double> U(0, 1000);
        std::normal_distribution<double> N(0, 0.05);
        const int nR = size[0] + size[1] + outR;
        Data d;
        d.L.resize(nL, 2);
        d.R.resize(nR, 2);
        for (int i = 0; i < nL; ++i)
            d.L(i, 0) = U(rng), d.L(i, 1) = U(rng);
        std::vector<int> perm(nR);
        std::iota(perm.begin(), perm.end(), 0);
        std::shuffle(perm.begin(), perm.end(), rng);
        int r = 0;
        for (int k = 0; k < 2; ++k)
        {
            const double c = std::cos(T[k].angle),
                         s = std::sin(T[k].angle);
            for (int i = first[k]; i < first[k] + size[k]; ++i, ++r)
            {
                const double x = d.L(i, 0), y = d.L(i, 1);
                d.R(perm[r], 0) = c * x - s * y + T[k].tx + N(rng);
                d.R(perm[r], 1) = s * x + c * y + T[k].ty + N(rng);
                d.truth[k].insert({i, perm[r]});
            }
        }
        for (; r < nR; ++r)
            d.R(perm[r], 0) = U(rng) * 2 - 500,
                         d.R(perm[r], 1) = U(rng) * 2 - 500;
        return d;
    }

    int hits(const std::vector<Correspondence>& pairs,
             const std::set<std::pair<int, int>>& truth)
    {
        int n = 0;
        for (const Correspondence& c : pairs)
            n += int(truth.count({c.left, c.right}));
        return n;
    }

    // |a| is copy k: nearly all of its pairs, nothing else, and the
    // transform of copy k
    bool isCopy(const PointAlignment& a, const Data& d, int k,
                const Rigid2D& T)
    {
        const int h = hits(a.pairs, d.truth[k]);
        const int n = int(d.truth[k].size());
        const PointTransform& t = a.transform;
        return h >= n - 1 && h == int(a.pairs.size()) &&
               std::fabs(std::remainder(t.angle - T.angle,
                                        6.283185307179586)) < 1e-3 &&
               std::hypot(t.translation(0) - T.tx,
                          t.translation(1) - T.ty) < 1.0;
    }

    PointResult align(const Data& d, DistinctMode mode, size_t results)
    {
        PointOptions po;  // rigid
        CliqueOptions co;
        co.results = results;
        co.distinct = mode;
        co.numWorkers = 4;
        return alignPoints(d.L, d.R, PointCols{}, po, co);
    }
}  // namespace

int main()
{
    const Rigid2D T[2] = {{0.4, 1200, -300}, {-2.1, -800, 1500}};
    const char* modeName[2] = {"explained", "disjoint"};
    const DistinctMode modes[2] = {DistinctMode::Explained,
                                   DistinctMode::Disjoint};
    for (unsigned seed = 1; seed <= 5; ++seed)
    {
        // separate: left 0..19 -> copy 0, left 20..35 -> copy 1
        {
            const int first[2] = {0, 20}, size[2] = {20, 16};
            const Data d = make(seed, first, size, 46, 10, T);
            for (int m = 0; m < 2; ++m)
            {
                const PointResult r = align(d, modes[m], 3);
                const bool ok =
                    r.status == Status::Ok && r.alignments.size() >= 2;
                CHECKF(ok && isCopy(r.alignments[0], d, 0, T[0]) &&
                           isCopy(r.alignments[1], d, 1, T[1]),
                       "separate seed %u %s: status %d, %zu results",
                       seed, modeName[m], int(r.status),
                       r.alignments.size());
                if (ok && modes[m] == DistinctMode::Explained)
                    CHECK(r.alignments[0].explained >=
                          int32_t(r.alignments[0].pairs.size()));
                if (ok && modes[m] == DistinctMode::Disjoint)
                    CHECK(r.alignments[1].explained ==
                          int32_t(r.alignments[1].pairs.size()));
                // fewer results: the same first one
                const PointResult r1 = align(d, modes[m], 1);
                CHECK(ok && r1.status == Status::Ok &&
                      r1.alignments.size() == 1 &&
                      r1.alignments[0].pairs == r.alignments[0].pairs);
            }
        }
        // shared: left 0..19 -> both copies
        {
            const int first[2] = {0, 0}, size[2] = {20, 20};
            const Data d = make(100 + seed, first, size, 30, 10, T);
            const PointResult ex = align(d, DistinctMode::Explained, 2);
            const bool ok =
                ex.status == Status::Ok && ex.alignments.size() == 2;
            // equal sizes: either copy may come first
            CHECKF(ok && ((isCopy(ex.alignments[0], d, 0, T[0]) &&
                           isCopy(ex.alignments[1], d, 1, T[1])) ||
                          (isCopy(ex.alignments[0], d, 1, T[1]) &&
                           isCopy(ex.alignments[1], d, 0, T[0]))),
                   "shared seed %u explained: status %d, %zu results",
                   seed, int(ex.status), ex.alignments.size());
            const PointResult dj = align(d, DistinctMode::Disjoint, 2);
            CHECKF(dj.status == Status::Ok, "shared seed %u disjoint",
                   seed);
            if (dj.status != Status::Ok) continue;
            const PointAlignment& a0 = dj.alignments[0];
            const int k =
                hits(a0.pairs, d.truth[0]) >= hits(a0.pairs, d.truth[1])
                    ? 0
                    : 1;
            CHECK(isCopy(a0, d, k, T[k]));
            for (size_t i = 1; i < dj.alignments.size(); ++i)
                CHECKF(hits(dj.alignments[i].pairs, d.truth[1 - k]) <
                           int(d.truth[1 - k].size()) / 2,
                       "shared seed %u disjoint: result %zu is the "
                       "other copy",
                       seed, i);
        }
    }
    return checkResult();
}
