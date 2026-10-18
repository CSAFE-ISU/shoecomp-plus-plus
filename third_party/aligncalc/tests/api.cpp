// Tier-1 API behaviour: Progress reports (monotonic, in [0, 1], only
// on the calling thread), cancellation -> Status::Cancelled, a Rules
// acceptTransform that rejects everything -> Status::NoMatch, invalid
// input -> Status::Error, impossible bounds -> Status::NoMatch, and
// rigid points in 1 and 3 dimensions.
#include "aligncalc/aligncalc.h"
#include "aligncalc/alignWithRules.h"
#include "check.h"
#include <atomic>
#include <cmath>
#include <mutex>
#include <random>
#include <string>
#include <thread>
using namespace AlignCalc;

namespace
{
    // records every report; cancels once a report reaches |cancelAt|
    // (<= 0: from the start)
    class Recorder : public Progress
    {
       public:
        explicit Recorder(float cancelAt = 2.0f)
            : cancelAt_(cancelAt), cancel_(cancelAt <= 0.0f)
        {
        }

        void report(float fraction, const char* stage) override
        {
            std::lock_guard<std::mutex> lock(m_);
            ++reports;
            if (std::this_thread::get_id() != caller_) offThread = true;
            if (!(fraction >= last && fraction <= 1.0f))
                monotonic = false;
            if (!stage || !*stage) noStage = true;
            last = fraction;
            if (fraction >= cancelAt_) cancel_ = true;
        }
        bool cancelled() const override
        {
            ++polls;
            return cancel_.load();
        }

        int reports = 0;
        float last = 0.0f;
        bool offThread = false, monotonic = true, noStage = false;
        mutable std::atomic<long> polls{0};

       private:
        std::mutex m_;
        const std::thread::id caller_ = std::this_thread::get_id();
        const float cancelAt_;
        std::atomic<bool> cancel_;
    };

    struct RejectAll : LabelRules
    {
        template <class Transform>
        bool acceptTransform(const Transform&) const
        {
            return false;
        }
    };

    // left = n random 2-D points, right = a similarity copy (rows
    // shuffled) plus |out| random points
    void points2D(unsigned seed, int n, int out, DoubleMatrixR& L,
                  DoubleMatrixR& R)
    {
        std::mt19937 rng(seed);
        std::uniform_real_distribution<double> U(0, 1000);
        L.resize(n, 2);
        R.resize(n + out, 2);
        std::vector<int> perm(n + out);
        for (int i = 0; i < n + out; ++i) perm[i] = i;
        std::shuffle(perm.begin(), perm.end(), rng);
        const double s = 1.3, c = s * std::cos(0.8),
                     sn = s * std::sin(0.8);
        for (int i = 0; i < n; ++i)
        {
            L(i, 0) = U(rng), L(i, 1) = U(rng);
            R(perm[i], 0) = c * L(i, 0) - sn * L(i, 1) + 40;
            R(perm[i], 1) = sn * L(i, 0) + c * L(i, 1) - 70;
        }
        for (int i = n; i < n + out; ++i)
            R(perm[i], 0) = U(rng), R(perm[i], 1) = U(rng);
    }

    // parallel-ish 3-D segments, right = left moved by a small shift
    void lines3D(unsigned seed, int n, DoubleMatrixR& L,
                 DoubleMatrixR& R)
    {
        std::mt19937 rng(seed);
        std::uniform_real_distribution<double> P(0, 2e-3),
            A(-0.02, 0.02);
        L.resize(n, 7);
        R.resize(n, 7);
        for (int i = 0; i < n; ++i)
        {
            const Eigen::Vector3d m(P(rng), P(rng), P(rng) * 0.1),
                d = Eigen::Vector3d(1, A(rng), A(rng)).normalized();
            for (int k = 0; k < 3; ++k)
            {
                L(i, k) = m(k), L(i, 3 + k) = d(k);
                R(i, k) = m(k) + 1e-4 * (k + 1), R(i, 3 + k) = d(k);
            }
            L(i, 6) = R(i, 6) = 5e-4;
        }
    }

    LineParams lineParams()
    {
        LineParams lp;
        lp.angleTol = 0.01;
        lp.sepTol = 2e-6;
        return lp;
    }

    void checkProgress(const char* what, const Recorder& rec)
    {
        CHECKF(rec.reports >= 3, "%s: %d reports", what, rec.reports);
        CHECKF(rec.monotonic, "%s: progress not monotonic in [0, 1]",
               what);
        CHECKF(!rec.offThread, "%s: report() off the calling thread",
               what);
        CHECKF(!rec.noStage, "%s: report() without a stage text", what);
        CHECKF(rec.polls > 0, "%s: cancelled() never polled", what);
    }

    void testProgress()
    {
        DoubleMatrixR L, R;
        points2D(3, 30, 30, L, R);
        CliqueOptions co;
        co.numWorkers = 4;
        co.results = 3;
        for (PointMode mode : {PointMode::Rigid, PointMode::Similarity})
        {
            PointOptions po;
            po.mode = mode;
            Recorder rec;
            const PointResult r =
                alignPoints(L, R, PointCols{}, po, co, &rec);
            const bool sim = mode == PointMode::Similarity;
            checkProgress(sim ? "similarity" : "rigid", rec);
            // the copy is scaled: only the similarity search finds it
            if (sim)
                CHECKF(r.status == Status::Ok &&
                           r.alignments[0].pairs.size() == 30 &&
                           std::fabs(r.alignments[0].transform.scale -
                                     1.3) < 1e-6,
                       "similarity: status %d", int(r.status));
        }
        DoubleMatrixR LL, LR;
        lines3D(4, 30, LL, LR);
        Recorder rec;
        const LineResult lr =
            alignLines3D(LL, LR, LineCols{}, lineParams(), co, &rec);
        checkProgress("lines", rec);
        CHECKF(lr.status == Status::Ok, "lines: status %d '%s'",
               int(lr.status), lr.message.c_str());
    }

    void testCancel()
    {
        DoubleMatrixR L, R, LL, LR;
        points2D(5, 30, 30, L, R);
        lines3D(6, 40, LL, LR);
        CliqueOptions co;
        co.numWorkers = 4;
        // cancelled from the start, while building the graph, and
        // during the clique search
        for (float at : {0.0f, 0.3f, 0.51f})
        {
            for (PointMode mode :
                 {PointMode::Rigid, PointMode::Similarity})
            {
                PointOptions po;
                po.mode = mode;
                Recorder rec(at);
                const PointResult r =
                    alignPoints(L, R, PointCols{}, po, co, &rec);
                CHECKF(r.status == Status::Cancelled &&
                           r.alignments.empty() && !r.message.empty(),
                       "points mode %d cancel at %.2f: status %d",
                       int(mode), at, int(r.status));
            }
            Recorder rec(at);
            const LineResult r = alignLines3D(LL, LR, LineCols{},
                                              lineParams(), co, &rec);
            CHECKF(
                r.status == Status::Cancelled && r.alignments.empty(),
                "lines cancel at %.2f: status %d", at, int(r.status));
        }
    }

    void testRejectAll()
    {
        DoubleMatrixR L, R;
        points2D(7, 30, 10, L, R);
        Progress none;
        CliqueOptions co;
        PointOptions po;
        po.mode = PointMode::Similarity;
        const PointResult r = alignPointsWithRules(
            L, R, PointCols{}, RejectAll{}, po, co, none);
        CHECKF(r.status == Status::NoMatch && r.alignments.empty() &&
                   !r.message.empty(),
               "points reject all: status %d", int(r.status));
        // the same data with LabelRules matches
        CHECK(alignPointsWithRules(L, R, PointCols{}, LabelRules{}, po,
                                   co, none)
                  .status == Status::Ok);

        DoubleMatrixR LL, LR;
        lines3D(8, 30, LL, LR);
        const LineResult lr = alignLines3DWithRules(
            LL, LR, LineCols{}, RejectAll{}, lineParams(), co, none);
        CHECKF(lr.status == Status::NoMatch && lr.alignments.empty(),
               "lines reject all: status %d", int(lr.status));
    }

    void testInvalid()
    {
        DoubleMatrixR L, R;
        points2D(9, 20, 0, L, R);
        CliqueOptions co;
        PointOptions sim;
        sim.mode = PointMode::Similarity;

        DoubleMatrixR L3(20, 3), R3(20, 3);
        L3.setRandom();
        R3.setRandom();
        PointCols c3;
        c3.dim = 3;
        PointResult r = alignPoints(L3, R3, c3, sim, co);
        CHECKF(r.status == Status::Error && !r.message.empty(),
               "3-D similarity: status %d", int(r.status));

        PointCols outside;  // y column past the matrix
        outside.pos = {0, 5, 2};
        r = alignPoints(L, R, outside, PointOptions{}, co);
        CHECKF(r.status == Status::Error && !r.message.empty(),
               "bad columns: status %d", int(r.status));

        PointCols noDim;
        noDim.dim = 4;
        r = alignPoints(L, R, noDim, PointOptions{}, co);
        CHECKF(r.status == Status::Error, "dim 4: status %d",
               int(r.status));

        LineCols badLines;  // 2-D rows have no direction columns
        const LineResult lr =
            alignLines3D(L, R, badLines, lineParams(), co);
        CHECKF(lr.status == Status::Error && !lr.message.empty(),
               "bad line columns: status %d", int(lr.status));

        // more pairs required than either side has
        CliqueOptions many;
        many.minMatched = 21;
        r = alignPoints(L, R, PointCols{}, sim, many);
        CHECKF(r.status == Status::NoMatch && !r.message.empty(),
               "minMatched > counts: status %d", int(r.status));
        // maxMatched below minMatched
        CliqueOptions inverted;
        inverted.minMatched = 5;
        inverted.maxMatched = 4;
        r = alignPoints(L, R, PointCols{}, sim, inverted);
        CHECKF(r.status == Status::NoMatch,
               "maxMatched < minMatched: status %d", int(r.status));
    }

    // 25 true pairs plus 8 random rows on each side
    void testRigidND()
    {
        std::mt19937 rng(5);
        std::uniform_real_distribution<double> U(-50, 50);
        std::normal_distribution<double> N(0, 0.01);
        const Eigen::Matrix3d rot =
            Eigen::AngleAxisd(0.7,
                              Eigen::Vector3d(1, 2, 3).normalized())
                .toRotationMatrix();
        const Eigen::Vector3d t3(5, -3, 2);
        for (int dim : {1, 3})
        {
            const int n = 25, out = 8;
            DoubleMatrixR L(n + out, dim), R(n + out, dim);
            for (int i = 0; i < n + out; ++i)
            {
                const Eigen::Vector3d p(U(rng), U(rng), U(rng));
                Eigen::Vector3d q =
                    dim == 3 ? Eigen::Vector3d(rot * p + t3)
                             : Eigen::Vector3d(p(0) + 7, 0, 0);
                if (i >= n) q = Eigen::Vector3d(U(rng), U(rng), U(rng));
                for (int d = 0; d < dim; ++d)
                    L(i, d) = p(d), R(i, d) = q(d) + N(rng);
            }
            PointCols cols;
            cols.dim = dim;
            PointOptions po;
            po.epsilon = 0.01;
            CliqueOptions co;
            co.minMatched = 5;
            const PointResult r = alignPoints(L, R, cols, po, co);
            if (!CHECKF(r.status == Status::Ok,
                        "dim %d: status %d '%s'", dim, int(r.status),
                        r.message.c_str()))
                continue;
            const PointAlignment& a = r.alignments[0];
            int correct = 0;
            for (const Correspondence& c : a.pairs)
                correct += c.left == c.right && c.left < n;
            // 1-D distances are coarse: a few outliers fit by chance
            CHECKF(correct >= (dim == 3 ? n : 20) &&
                       correct >= int(a.pairs.size()) - 1,
                   "dim %d: %d true of %zu pairs", dim, correct,
                   a.pairs.size());
            CHECK(a.transform.scale == 1.0);
            CHECK(a.transform.rotation.rows() == dim &&
                  a.transform.translation.size() == dim);
            if (dim == 1)
                CHECKF(std::fabs(a.transform.translation(0) - 7) < 0.05,
                       "dim 1: t %.3f", a.transform.translation(0));
            else
                CHECKF((a.transform.rotation - rot).norm() < 1e-3 &&
                           (a.transform.translation - t3).norm() < 0.05,
                       "dim 3: |dR| %.2e, |dt| %.2e",
                       (a.transform.rotation - rot).norm(),
                       (a.transform.translation - t3).norm());
        }
    }
}  // namespace

int main()
{
    testProgress();
    testCancel();
    testRejectAll();
    testInvalid();
    testRigidND();
    return checkResult();
}
