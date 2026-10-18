// alignPoints on the seeded 2-D datasets (datasets.h), rigid and
// similarity: the planted correspondences appear in a result, their
// fit recovers the planted transform, every result is a one-to-one
// set of pairs, and the results do not depend on the worker count.
#include "aligncalc/aligncalc.h"
#include "check.h"
#include "datasets.h"
#include <algorithm>
#include <cmath>
using namespace AlignCalc;

constexpr double kPi = 3.14159265358979323846;

namespace
{
    std::vector<int> codes(const std::vector<Correspondence>& pairs)
    {
        std::vector<int> v;
        for (const Correspondence& c : pairs)
            v.push_back(c.left * 1000 + c.right);
        std::sort(v.begin(), v.end());
        return v;
    }

    bool oneToOne(const std::vector<Correspondence>& pairs, int nL,
                  int nR)
    {
        std::vector<char> usedL(nL, 0), usedR(nR, 0);
        for (const Correspondence& c : pairs)
        {
            if (c.left < 0 || c.left >= nL || c.right < 0 ||
                c.right >= nR || usedL[c.left] || usedR[c.right])
                return false;
            usedL[c.left] = usedR[c.right] = 1;
        }
        return true;
    }

    bool sameResult(const PointResult& a, const PointResult& b)
    {
        if (a.status != b.status ||
            a.alignments.size() != b.alignments.size())
            return false;
        for (size_t i = 0; i < a.alignments.size(); ++i)
        {
            const PointAlignment &x = a.alignments[i],
                                 &y = b.alignments[i];
            if (x.pairs != y.pairs || x.explained != y.explained ||
                x.transform.scale != y.transform.scale ||
                x.transform.rotation != y.transform.rotation ||
                x.transform.translation != y.transform.translation)
                return false;
        }
        return true;
    }
}  // namespace

int main()
{
    const std::vector<Dataset> ds = datasets();
    PointCols cols;
    cols.label = 2;
    int withResult = 0, recovered = 0;
    double maxRot = 0, maxScale = 0, maxT = 0;
    for (size_t id = 0; id < ds.size(); ++id)
    {
        const Dataset& d = ds[id];
        PointOptions po;
        po.mode =
            d.sameScale ? PointMode::Rigid : PointMode::Similarity;
        CliqueOptions co;
        co.maxMatched = 500;
        co.numWorkers = 1;
        const PointResult r1 = alignPoints(d.L, d.R, cols, po, co);
        for (size_t w : {3, 8})
        {
            co.numWorkers = w;
            CHECKF(sameResult(r1, alignPoints(d.L, d.R, cols, po, co)),
                   "dataset %zu: 1 vs %zu workers differ", id, w);
        }
        CHECKF(r1.status == Status::Ok || r1.status == Status::NoMatch,
               "dataset %zu: status %d '%s'", id, int(r1.status),
               r1.message.c_str());
        if (r1.status != Status::Ok) continue;
        CHECK(!r1.alignments.empty() && r1.message.empty());
        ++withResult;
        for (const PointAlignment& a : r1.alignments)
        {
            CHECKF(oneToOne(a.pairs, d.n, d.n) && a.pairs.size() >= 3,
                   "dataset %zu", id);
            CHECKF(a.explained >= int32_t(a.pairs.size()),
                   "dataset %zu: explained %d < %zu pairs", id,
                   a.explained, a.pairs.size());
            if (d.sameScale) CHECK(a.transform.scale == 1.0);
        }
        for (const PointAlignment& a : r1.alignments)
        {
            const std::vector<int> got = codes(a.pairs);
            if (!std::includes(got.begin(), got.end(),
                               d.planted.begin(), d.planted.end()))
                continue;
            ++recovered;
            const PointTransform& t = a.transform;
            maxRot = std::max(maxRot, std::fabs(std::remainder(
                                          t.angle - d.rot, 2 * kPi)));
            maxScale = std::max(maxScale,
                                std::fabs(t.scale - d.scale) / d.scale);
            maxT = std::max(maxT, std::hypot(t.translation(0) - d.tx,
                                             t.translation(1) - d.ty));
            break;
        }
    }
    printf(
        "planted set found in %d/%d (%zu datasets); max |drot| "
        "%.2e rad, |dscale|/scale %.2e, |dt| %.3f\n",
        recovered, withResult, ds.size(), maxRot, maxScale, maxT);
    // 62/78 when these tests were written; the rest are small,
    // ambiguous sets
    CHECKF(recovered >= 62, "planted set found in %d", recovered);
    CHECK(maxRot < 1e-3);
    CHECK(maxScale < 1e-3);
    CHECK(maxT < 1.0);
    return checkResult();
}
