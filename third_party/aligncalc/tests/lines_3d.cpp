// alignLines3D on synthetic segments: right = a rigid copy of a
// bundle of nearly parallel left segments (rows shuffled, some
// directions flipped) plus unrelated segments on both sides. The
// copy and its transform are recovered; with a label column only
// equal labels match, and shifted labels leave nothing to match.
#include "aligncalc/aligncalc.h"
#include "check.h"
#include <cmath>
#include <numeric>
#include <random>
using namespace AlignCalc;

namespace
{
    constexpr int kLabelCol = 7;

    struct Data
    {
        DoubleMatrixR L, R;
        std::vector<int> truth;  // right row of left row i, or -1
        Eigen::Isometry3d T;
    };

    // rows (mid xyz, dir xyz, length, label); x3p-like scale (metres)
    Data make(unsigned seed, int n, int extra)
    {
        std::mt19937 rng(seed);
        std::uniform_real_distribution<double> P(0, 2e-3), A(-0.2, 0.2),
            U(0, 1);
        Data d;
        d.T = Eigen::Isometry3d::Identity();
        d.T.rotate(Eigen::AngleAxisd(
            0.3, Eigen::Vector3d(0.2, -0.1, 1).normalized()));
        d.T.translation() << 1e-4, 2e-4, -5e-5;
        const int nL = n + extra, nR = n + extra;
        d.L.setZero(nL, 8);
        d.R.setZero(nR, 8);
        d.truth.assign(nL, -1);
        std::vector<int> perm(nR);
        std::iota(perm.begin(), perm.end(), 0);
        std::shuffle(perm.begin(), perm.end(), rng);
        auto segment = [&](DoubleMatrixR& m, int row,
                           const Eigen::Vector3d& mid,
                           const Eigen::Vector3d& dir, double len)
        {
            for (int k = 0; k < 3; ++k)
                m(row, k) = mid(k), m(row, 3 + k) = dir(k);
            m(row, 6) = len;
        };
        auto randomSegment =
            [&](Eigen::Vector3d& mid, Eigen::Vector3d& dir)
        {
            mid = Eigen::Vector3d(P(rng), P(rng), P(rng) * 0.1);
            dir = Eigen::Vector3d(1, A(rng) * 0.1, A(rng) * 0.05)
                      .normalized();
        };
        for (int i = 0; i < n; ++i)
        {
            Eigen::Vector3d mid, dir;
            randomSegment(mid, dir);
            const double len = 3e-4 + 4e-4 * U(rng);
            segment(d.L, i, mid, dir, len);
            const Eigen::Vector3d dr =
                (i % 2 ? -1.0 : 1.0) * (d.T.linear() * dir);
            segment(d.R, perm[i], d.T * mid, dr, len);
            d.truth[i] = perm[i];
        }
        for (int i = n; i < nL; ++i)
        {
            Eigen::Vector3d mid, dir;
            randomSegment(mid, dir);
            segment(d.L, i, mid, dir, 5e-4);
            randomSegment(mid, dir);
            segment(d.R, perm[i], d.T * mid, d.T.linear() * dir, 5e-4);
        }
        return d;
    }

    LineParams params()
    {
        LineParams lp;
        lp.angleTol = 0.01;
        lp.sepTol = 2e-6;
        return lp;
    }

    double rotationError(const Eigen::Isometry3d& a,
                         const Eigen::Isometry3d& b)
    {
        return Eigen::AngleAxisd(a.linear().transpose() * b.linear())
            .angle();
    }
}  // namespace

int main()
{
    const int n = 30, extra = 10;
    for (unsigned seed = 1; seed <= 5; ++seed)
    {
        Data d = make(seed, n, extra);
        CliqueOptions co;
        co.minMatched = 5;
        co.numWorkers = 4;

        // no labels
        const LineResult r =
            alignLines3D(d.L, d.R, LineCols{}, params(), co);
        CHECKF(r.status == Status::Ok && !r.alignments.empty(),
               "seed %u: status %d '%s'", seed, int(r.status),
               r.message.c_str());
        if (r.status == Status::Ok)
        {
            const LineAlignment& a = r.alignments[0];
            int correct = 0;
            for (const Correspondence& c : a.pairs)
                correct += d.truth[c.left] == c.right;
            CHECKF(
                correct >= n - 2 && correct >= int(a.pairs.size()) - 1,
                "seed %u: %d of %zu pairs correct", seed, correct,
                a.pairs.size());
            CHECKF(
                rotationError(a.transform, d.T) < 1e-4 &&
                    (a.transform.translation() - d.T.translation())
                            .norm() < 1e-6,
                "seed %u: rotation error %.2e, |dt| %.2e", seed,
                rotationError(a.transform, d.T),
                (a.transform.translation() - d.T.translation()).norm());
            CHECK(a.explained >= int32_t(a.pairs.size()));
        }

        // labels 0..2; only equal labels match
        for (int i = 0; i < d.L.rows(); ++i) d.L(i, kLabelCol) = i % 3;
        for (int i = 0; i < d.L.rows(); ++i)
            if (d.truth[i] >= 0) d.R(d.truth[i], kLabelCol) = i % 3;
        LineCols cols;
        cols.label = kLabelCol;
        const LineResult rl =
            alignLines3D(d.L, d.R, cols, params(), co);
        CHECKF(rl.status == Status::Ok, "seed %u labels: status %d",
               seed, int(rl.status));
        for (const LineAlignment& a : rl.alignments)
            for (const Correspondence& c : a.pairs)
                CHECK(d.L(c.left, kLabelCol) ==
                      d.R(c.right, kLabelCol));
        if (rl.status == Status::Ok)
        {
            int correct = 0;
            for (const Correspondence& c : rl.alignments[0].pairs)
                correct += d.truth[c.left] == c.right;
            CHECKF(correct >= n - 2, "seed %u labels: %d correct", seed,
                   correct);
        }

        // shifted labels: no true pair is admissible
        for (int i = 0; i < d.R.rows(); ++i)
            d.R(i, kLabelCol) = std::fmod(d.R(i, kLabelCol) + 1, 3);
        const LineResult rs =
            alignLines3D(d.L, d.R, cols, params(), co);
        for (const LineAlignment& a : rs.alignments)
            for (const Correspondence& c : a.pairs)
                CHECK(d.truth[c.left] != c.right &&
                      d.L(c.left, kLabelCol) ==
                          d.R(c.right, kLabelCol));
        CHECKF(rs.status == Status::NoMatch,
               "seed %u shifted labels: status %d, %zu results", seed,
               int(rs.status), rs.alignments.size());
    }
    return checkResult();
}
