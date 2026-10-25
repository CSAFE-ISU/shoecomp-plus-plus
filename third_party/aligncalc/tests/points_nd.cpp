// Similarity matching in 3, 4 and 6 dimensions: a planted set under a
// random rotation, scale and translation (right rows shuffled, random
// outliers on both sides) is matched by matchPoints with and without
// the scale anchor, independent of the worker count; alignPoints
// recovers the transform; a scale outside scaleRange is not matched;
// a mirror-image copy is matched (distances cannot see reflection).
#include "aligncalc/aligncalc.h"
#include "check.h"
#include <Eigen/Dense>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
using namespace AlignCalc;

namespace
{
    struct Planted
    {
        DoubleMatrixR L, R;
        std::vector<int> truth;  // left row -> right row, -1 = outlier
        Eigen::MatrixXd rot;
        double scale = 1.0;
        Eigen::VectorXd t;
    };

    // n planted pairs plus |out| random rows on each side
    Planted plant(int dim, int n, int out, double scale, bool mirror,
                  unsigned seed)
    {
        std::mt19937 rng(seed);
        std::uniform_real_distribution<double> U(-50, 50);
        std::normal_distribution<double> G(0, 1), N(0, 0.002);
        Planted p;
        Eigen::MatrixXd A(dim, dim);
        for (int i = 0; i < dim; ++i)
            for (int j = 0; j < dim; ++j) A(i, j) = G(rng);
        Eigen::MatrixXd Q =
            Eigen::HouseholderQR<Eigen::MatrixXd>(A).householderQ();
        if (Q.determinant() < 0) Q.col(0) *= -1.0;
        if (mirror) Q.row(0) *= -1.0;
        p.rot = Q;
        p.scale = scale;
        p.t = Eigen::VectorXd(dim);
        for (int d = 0; d < dim; ++d) p.t(d) = U(rng);

        const int rows = n + out;
        std::vector<int> perm(rows);
        std::iota(perm.begin(), perm.end(), 0);
        std::shuffle(perm.begin(), perm.end(), rng);
        p.L.resize(rows, dim);
        p.R.resize(rows, dim);
        p.truth.assign(rows, -1);
        for (int i = 0; i < rows; ++i)
        {
            Eigen::VectorXd x(dim), y(dim);
            for (int d = 0; d < dim; ++d) x(d) = U(rng);
            if (i < n)
            {
                y = scale * Q * x + p.t;
                p.truth[i] = perm[i];
            }
            else
                for (int d = 0; d < dim; ++d) y(d) = 2.0 * U(rng);
            for (int d = 0; d < dim; ++d)
                p.L(i, d) = x(d), p.R(perm[i], d) = y(d) + N(rng);
        }
        return p;
    }

    // planted pairs in |pairs|, and pairs that are not planted
    void score(const Planted& p,
               const std::vector<Correspondence>& pairs, int& right,
               int& wrong)
    {
        right = wrong = 0;
        for (const Correspondence& c : pairs)
            (p.truth[c.left] == c.right ? right : wrong)++;
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

    PointOptions similarity(double anchorTol)
    {
        PointOptions po;
        po.mode = PointMode::Similarity;
        po.epsilon = 0.01;
        po.anchorTol = anchorTol;
        return po;
    }
}  // namespace

int main()
{
    const int n = 30, out = 10;
    for (int dim : {3, 4, 6})
    {
        const Planted p = plant(dim, n, out, 2.5, false, 11 + dim);
        PointCols cols;
        cols.dim = dim;
        CliqueOptions co;
        co.minMatched = 5;
        for (double anchorTol : {0.05, 0.0})
        {
            // distances concentrate as dim grows, so triangles look
            // alike and the graph densifies (6-D: ~0.58 here); without
            // the anchor the clique search takes minutes
            if (anchorTol == 0.0 && dim > 4) continue;
            const MatchResult m =
                matchPoints(p.L, p.R, cols, similarity(anchorTol), co);
            if (!CHECKF(m.status == Status::Ok && !m.matches.empty(),
                        "dim %d anchor %g: status %d '%s'", dim,
                        anchorTol, int(m.status), m.message.c_str()))
                continue;
            int right, wrong;
            score(p, m.matches[0].pairs, right, wrong);
            CHECKF(right >= n - 1 && wrong <= 1,
                   "dim %d anchor %g: %d planted, %d wrong", dim,
                   anchorTol, right, wrong);
        }

        CliqueOptions one = co, four = co;
        one.numWorkers = 1;
        four.numWorkers = 4;
        CHECKF(sameMatches(
                   matchPoints(p.L, p.R, cols, similarity(0.05), one),
                   matchPoints(p.L, p.R, cols, similarity(0.05), four)),
               "dim %d: 1 and 4 workers differ", dim);

        // scale 2.5 outside [1/2, 2]: no match holds planted pairs
        PointOptions narrow = similarity(0.05);
        narrow.scaleRange = 2.0;
        const MatchResult nm = matchPoints(p.L, p.R, cols, narrow, co);
        int planted = 0;
        for (const PointMatch& x : nm.matches)
        {
            int right, wrong;
            score(p, x.pairs, right, wrong);
            planted = std::max(planted, right);
        }
        CHECKF(nm.status != Status::Error && planted < 5,
               "dim %d scale range 2: status %d, %d planted", dim,
               int(nm.status), planted);
    }

    // alignPoints in 4-D recovers the transform
    {
        const Planted p = plant(4, n, out, 0.4, false, 7);
        PointCols cols;
        cols.dim = 4;
        CliqueOptions co;
        co.minMatched = 5;
        const PointResult r =
            alignPoints(p.L, p.R, cols, similarity(0.05), co);
        if (CHECKF(r.status == Status::Ok, "4-D align: status %d '%s'",
                   int(r.status), r.message.c_str()))
        {
            const PointTransform& t = r.alignments[0].transform;
            int right, wrong;
            score(p, r.alignments[0].pairs, right, wrong);
            CHECKF(right >= n - 1 && wrong <= 1,
                   "4-D align: %d planted, %d wrong", right, wrong);
            CHECKF(std::fabs(t.scale / p.scale - 1) < 1e-3 &&
                       (t.rotation - p.rot).norm() < 1e-3 &&
                       (t.translation - p.t).norm() < 0.05,
                   "4-D align: scale %.5f (%.5f), |dR| %.2e, |dt| %.2e",
                   t.scale, p.scale, (t.rotation - p.rot).norm(),
                   (t.translation - p.t).norm());
        }
    }

    // a mirror image is matched too, with the anchor on
    {
        const Planted p = plant(3, n, out, 1.7, true, 3);
        PointCols cols;
        cols.dim = 3;
        CliqueOptions co;
        co.minMatched = 5;
        const MatchResult m =
            matchPoints(p.L, p.R, cols, similarity(0.05), co);
        int right = 0, wrong = 0;
        if (m.status == Status::Ok && !m.matches.empty())
            score(p, m.matches[0].pairs, right, wrong);
        CHECKF(right >= n - 1 && wrong <= 1,
               "3-D mirror: status %d, %d planted, %d wrong",
               int(m.status), right, wrong);
    }
    return checkResult();
}
