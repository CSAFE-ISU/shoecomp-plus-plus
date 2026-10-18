// The anchored 2-D search (PointOptions::anchorTol) against
// mirror-image matches: right = a copy of k1 left points plus a
// mirrored (reflected) copy of k2 > k1 others, plus outliers. Distances
// cannot tell a reflection apart, so the plain search prefers the
// larger mirror set; the anchor (no reflection) must pick the copy.
#include "aligncalc/aligncalc.h"
#include "check.h"
#include <cmath>
#include <random>
#include <set>
using namespace AlignCalc;

constexpr double kPi = 3.14159265358979323846;

namespace
{
    enum class Pick
    {
        Copy,
        Mirror,
        Other,
        None
    };

    // seeded case; similarity copies are scaled by 1.4
    Pick run(bool rigid, double anchorTol, unsigned seed)
    {
        const int k1 = 10, k2 = 15, nOut = 15, n = k1 + k2 + nOut;
        std::mt19937 rng(seed);
        std::uniform_real_distribution<double> U(0, 1000), A(-kPi, kPi);
        std::normal_distribution<double> N(0, 0.2);
        DoubleMatrixR L(n, 2), R(n, 2);
        for (int i = 0; i < n; ++i) L(i, 0) = U(rng), L(i, 1) = U(rng);
        for (int i = 0; i < n; ++i)
            R(i, 0) = U(rng) + 500, R(i, 1) = U(rng) + 500;
        const double s = rigid ? 1.0 : 1.4, th = A(rng),
                     c = s * std::cos(th), sn = s * std::sin(th);
        // left i <-> right i: copy for i < k1, mirror up to k1 + k2
        for (int i = 0; i < k1 + k2; ++i)
        {
            const double x = L(i, 0), y = i < k1 ? L(i, 1) : -L(i, 1);
            R(i, 0) = c * x - sn * y + 300 + N(rng);
            R(i, 1) = sn * x + c * y + 200 + N(rng);
        }
        PointOptions po;
        po.mode = rigid ? PointMode::Rigid : PointMode::Similarity;
        po.scaleRange = 3;
        po.epsilon = 0.005;
        po.anchorTol = anchorTol;
        CliqueOptions co;
        co.numWorkers = 4;
        co.maxMatched = n;
        const PointResult r = alignPoints(L, R, PointCols{}, po, co);
        if (r.status != Status::Ok) return Pick::None;
        int copy = 0, mirror = 0;
        const auto& best = r.alignments[0].pairs;
        for (const Correspondence& x : best)
        {
            if (x.left != x.right) continue;
            if (x.left < k1)
                ++copy;
            else if (x.left < k1 + k2)
                ++mirror;
        }
        const int m = int(best.size());
        if (copy >= k1 - 1 && copy >= m - 1) return Pick::Copy;
        if (mirror > m / 2) return Pick::Mirror;
        return Pick::Other;
    }
}  // namespace

int main()
{
    const int seeds = 10;
    for (bool rigid : {true, false})
        for (double tol : {0.0, 0.01})
        {
            int count[4] = {0, 0, 0, 0};
            for (int seed = 0; seed < seeds; ++seed)
                ++count[int(run(rigid, tol, 100 + seed))];
            printf(
                "%-10s anchorTol %.2f: copy %d, mirror %d, other %d, "
                "none %d\n",
                rigid ? "rigid" : "similarity", tol, count[0], count[1],
                count[2], count[3]);
            // 9 and 10 of 10 when these tests were written
            if (tol > 0)
                CHECKF(count[int(Pick::Copy)] >= 9,
                       "anchored %s search picked the copy %d/%d",
                       rigid ? "rigid" : "similarity",
                       count[int(Pick::Copy)], seeds);
            // without the anchor the mirror must win sometimes, or
            // this test proves nothing (rigid: 10/10, similarity 6/10)
            else
                CHECK(count[int(Pick::Mirror)] >= 3);
        }
    return checkResult();
}
