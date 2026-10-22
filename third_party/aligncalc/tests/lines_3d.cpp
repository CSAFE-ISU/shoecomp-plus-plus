// alignLines3D on synthetic segments: right = a rigid copy of a
// bundle of nearly parallel left segments (rows shuffled, some
// directions flipped) plus unrelated segments on both sides. The
// copy and its transform are recovered; with a label column only
// equal labels match, and shifted labels leave nothing to match.
// angleTol = 0 turns the pair angle check off. LineAnchor3D keeps
// lines that fit the anchor's transform across and in depth.
#include "aligncalc/aligncalc.h"
#include "aligncalc/lineAnchor3d.h"
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
        lp.anchorTol = 0.0;
        return lp;
    }

    double rotationError(const Eigen::Isometry3d& a,
                         const Eigen::Isometry3d& b)
    {
        return Eigen::AngleAxisd(a.linear().transpose() * b.linear())
            .angle();
    }
}  // namespace

// Two right lines at the left pair's separation but 10 deg apart
// (left: parallel): an edge only with the angle check off.
void angleOff()
{
    DoubleMatrixR L = DoubleMatrixR::Zero(2, 7), R = L;
    L(0, 3) = L(1, 3) = R(0, 3) = 1.0;
    L(1, 1) = R(1, 1) = 1e-4;
    const double a = 10 * 3.14159265358979323846 / 180;
    R(1, 3) = std::cos(a);
    R(1, 5) = std::sin(a);
    L(0, 6) = L(1, 6) = R(0, 6) = R(1, 6) = 5e-4;
    for (double tol : {0.01, 0.0})
    {
        LineParams lp = params();
        lp.angleTol = tol;
        const RigidLines3D<> model(L, R, LineCols{}, lp);
        std::vector<Correspondence> out;
        model.consistentWith({0, 0}, out);
        const bool edge =
            out.size() == 1 && out[0].left == 1 && out[0].right == 1;
        CHECKF(edge == (tol == 0.0), "angleTol %g: %zu edges", tol,
               out.size());
    }
}

// Parallel lines across a flat land (x = 0, 0.2, 0.5, 1 mm); the
// right copy moves line 3 by 3 um in depth (z), which the pairwise
// separation barely sees. Anchor (0, 1): line 2 kept, line 3 only
// with a depth tolerance above 3 um; flipped right directions too.
void lineAnchor()
{
    DoubleMatrixR L = DoubleMatrixR::Zero(4, 7);
    const double xs[4] = {0, 2e-4, 5e-4, 1e-3};
    for (int i = 0; i < 4; ++i)
    {
        L(i, 0) = xs[i];
        L(i, 4) = 1.0;
        L(i, 6) = 5e-4;
    }
    DoubleMatrixR R = L;
    R(3, 2) = 3e-6;
    const Correspondence cands[2] = {{2, 2}, {3, 3}};
    for (double sign : {1.0, -1.0})
    {
        DoubleMatrixR Rs = R;
        Rs.col(4) *= sign;
        for (double depth : {1e-6, 5e-6})
        {
            const LineAnchor3D anchor(L, Rs, LineCols{}, 4e-6, depth);
            uint8_t keep[2] = {9, 9};
            anchor.filter({0, 0}, {1, 1}, cands, 2, keep);
            CHECKF(keep[0] == 1 && keep[1] == (depth > 3e-6),
                   "sign %g depth %g: keep %d %d", sign, depth,
                   int(keep[0]), int(keep[1]));
        }
    }
}

int main()
{
    angleOff();
    lineAnchor();
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

        // anchored search
        LineParams anchored = params();
        anchored.anchorTol = 1e-6;
        const LineResult rn =
            alignLines3D(d.L, d.R, LineCols{}, anchored, co);
        CHECKF(rn.status == Status::Ok, "seed %u anchor: status %d",
               seed, int(rn.status));
        if (rn.status == Status::Ok)
        {
            int correct = 0;
            for (const Correspondence& c : rn.alignments[0].pairs)
                correct += d.truth[c.left] == c.right;
            CHECKF(correct >= n - 2, "seed %u anchor: %d correct", seed,
                   correct);
        }

        // separation only
        LineParams noAngle = params();
        noAngle.angleTol = 0.0;
        const LineResult ra =
            alignLines3D(d.L, d.R, LineCols{}, noAngle, co);
        CHECKF(ra.status == Status::Ok, "seed %u no angle: status %d",
               seed, int(ra.status));
        if (ra.status == Status::Ok)
        {
            int correct = 0;
            for (const Correspondence& c : ra.alignments[0].pairs)
                correct += d.truth[c.left] == c.right;
            CHECKF(correct >= n - 2, "seed %u no angle: %d correct",
                   seed, correct);
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
