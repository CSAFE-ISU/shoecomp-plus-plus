#include "aligncalc/similarity2d.h"
#include "rts/triangle2d.internal.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace AlignCalc
{
    namespace detail
    {
        namespace
        {
            // right vertex matched to left vertex v, for each
            // Triangle2D::compare check bit
            constexpr int kPerm[6][3] = {{0, 1, 2}, {0, 2, 1},
                                         {1, 0, 2}, {1, 2, 0},
                                         {2, 1, 0}, {2, 0, 1}};
            constexpr double kSlack = 1e-9;

            double dist(const DoubleMatrixR& m, const PointCols& c,
                        int32_t i, int32_t j)
            {
                double s = 0.0;
                for (int d = 0; d < c.dim; ++d)
                {
                    const double t = m(i, c.col(d)) - m(j, c.col(d));
                    s += t * t;
                }
                return std::sqrt(s);
            }

            // 2-D: same side formulas as Triangle2D::construct; the
            // side-ratio test needs only the side lengths, so it holds
            // in any dimension
            void sides(const DoubleMatrixR& m, const PointCols& c,
                       int32_t i, int32_t j, int32_t k,
                       RTS::Triangle2D& t)
            {
                if (c.dim != 2)
                {
                    t.as = dist(m, c, k, j);
                    t.bs = dist(m, c, i, k);
                    t.cs = dist(m, c, j, i);
                    return;
                }
                const int x = c.col(0);
                const int y = c.col(1);
                t.as = std::hypot(m(k, x) - m(j, x), m(k, y) - m(j, y));
                t.bs = std::hypot(m(i, x) - m(k, x), m(i, y) - m(k, y));
                t.cs = std::hypot(m(j, x) - m(i, x), m(j, y) - m(i, y));
            }

            // logs of the smallest and middle perimeter-normalized side
            void shape(const RTS::Triangle2D& t, double& lu1,
                       double& lu2)
            {
                double a = t.as, b = t.bs, c = t.cs;
                if (a > b) std::swap(a, b);
                if (b > c) std::swap(b, c);
                if (a > b) std::swap(a, b);
                const double p = a + b + c;
                lu1 = std::log(a / p);
                lu2 = std::log(b / p);
            }

            // Sorted circular gaps (smallest two) between the three
            // angles of a triangle; NaN if any angle is NaN.
            std::array<float, 2> angleGaps(const DoubleMatrixR& m,
                                           int col, int32_t i,
                                           int32_t j, int32_t k)
            {
                const float nan =
                    std::numeric_limits<float>::quiet_NaN();
                if (col < 0) return {nan, nan};
                double a[3] = {m(i, col), m(j, col), m(k, col)};
                for (double& v : a)
                {
                    if (std::isnan(v)) return {nan, nan};
                    v = std::fmod(v, 360.0);
                    if (v < 0) v += 360.0;
                }
                std::sort(a, a + 3);
                double g[3] = {a[1] - a[0], a[2] - a[1],
                               360.0 - (a[2] - a[0])};
                std::sort(g, g + 3);
                return {float(g[0]), float(g[1])};
            }

            int64_t cellKey(int64_t b1, int64_t b2)
            {
                return int64_t(uint64_t(b1) << 32) |
                       int64_t(uint32_t(int32_t(b2)));
            }
        }  // namespace

        // If a left/right pair passes BINARY_CMP, every side ratio
        // left/right lies in [t, t + eps] with t >= 1/scaleRange - eps,
        // so each normalized left side is within a factor
        // f = 1 + eps / t of its partner, and so are the sorted sides.
        // Right triangles are binned by (log u1, log u2) with bin width
        // log f; a query visits the bins within log f of the left
        // triangle and Triangle2D::compare decides. Without a positive
        // t the index cannot prune and every triangle is a candidate.
        struct SimilarityIndex2D::Impl
        {
            const DoubleMatrixR& L;
            const DoubleMatrixR& R;
            PointCols cols;
            SimilarityParams params;
            bool valid = false;
            bool brute = false;
            double logF = 0.0;
            double width = 1.0;
            std::vector<int64_t> keys;
            std::vector<std::array<int32_t, 3>> tris;
            std::vector<std::array<float, 2>> gaps;  // per tris entry
            bool oriented = false;

            Impl(const DoubleMatrixR& left, const DoubleMatrixR& right,
                 const PointCols& c, const SimilarityParams& p)
                : L(left), R(right), cols(c), params(p)
            {
                if (cols.dim < 2 || !cols.valid(L) || !cols.valid(R) ||
                    !(params.epsilon >= 0.0) ||
                    !(params.scaleRange > 0.0))
                    return;
                valid = true;
                oriented = cols.dim == 2 && params.angleCol >= 0 &&
                           params.angleCol < L.cols() &&
                           params.angleCol < R.cols() &&
                           params.angleTolDeg > 0.0;
                const double tmin =
                    1.0 / params.scaleRange - params.epsilon;
                if (tmin > 1e-12)
                {
                    logF = std::log1p(params.epsilon / tmin);
                    width = std::max(logF, 1e-6);
                }
                else
                    brute = true;

                const int32_t n = int32_t(R.rows());
                std::vector<std::pair<int64_t, std::array<int32_t, 3>>>
                    entries;
                RTS::Triangle2D t;
                for (int32_t p0 = 0; p0 < n; ++p0)
                    for (int32_t p1 = p0 + 1; p1 < n; ++p1)
                        for (int32_t p2 = p1 + 1; p2 < n; ++p2)
                        {
                            sides(R, cols, p0, p1, p2, t);
                            if (!t.valid()) continue;
                            int64_t key = 0;
                            if (!brute)
                            {
                                double lu1, lu2;
                                shape(t, lu1, lu2);
                                key = cellKey(bin(lu1), bin(lu2));
                            }
                            entries.push_back({key, {p0, p1, p2}});
                        }
                std::sort(entries.begin(), entries.end());
                keys.reserve(entries.size());
                tris.reserve(entries.size());
                for (const auto& e : entries)
                {
                    keys.push_back(e.first);
                    tris.push_back(e.second);
                }
                if (oriented)
                {
                    gaps.reserve(tris.size());
                    for (const auto& t : tris)
                        gaps.push_back(angleGaps(R, params.angleCol,
                                                 t[0], t[1], t[2]));
                }
            }

            int64_t bin(double lu) const
            {
                return int64_t(std::floor(lu / width));
            }

            void test(const RTS::Triangle2D& tl,
                      const std::array<float, 2>& gl,
                      const std::array<int32_t, 3>& l, size_t idx,
                      std::vector<TupleMatch<3>>& out) const
            {
                if (oriented && !std::isnan(gl[0]))
                {
                    const auto& gr = gaps[idx];
                    const double tol = params.angleTolDeg + 1e-3;
                    if (!std::isnan(gr[0]) &&
                        (std::fabs(gl[0] - gr[0]) > tol ||
                         std::fabs(gl[1] - gr[1]) > tol))
                        return;
                }
                const auto& tri = tris[idx];
                RTS::Triangle2D tr;
                sides(R, cols, tri[0], tri[1], tri[2], tr);
                uint8_t check[6];
                if (!tl.compare(tr, check, params.scaleRange,
                                params.epsilon))
                    return;
                for (int s = 0; s < 6; ++s)
                    if (check[s])
                        out.push_back(
                            {l,
                             {tri[kPerm[s][0]], tri[kPerm[s][1]],
                              tri[kPerm[s][2]]}});
            }
        };

        SimilarityIndex2D::SimilarityIndex2D(
            const DoubleMatrixR& left, const DoubleMatrixR& right,
            const PointCols& cols, const SimilarityParams& params)
            : impl_(std::make_unique<Impl>(left, right, cols, params))
        {
        }

        SimilarityIndex2D::~SimilarityIndex2D() = default;

        bool SimilarityIndex2D::valid() const { return impl_->valid; }

        void SimilarityIndex2D::match(
            const std::array<int32_t, 3>& l,
            std::vector<TupleMatch<3>>& out) const
        {
            const Impl& m = *impl_;
            if (!m.valid) return;
            RTS::Triangle2D tl;
            sides(m.L, m.cols, l[0], l[1], l[2], tl);
            if (!tl.valid()) return;
            const std::array<float, 2> gl =
                m.oriented ? angleGaps(m.L, m.params.angleCol, l[0],
                                       l[1], l[2])
                           : std::array<float, 2>{0.0f, 0.0f};
            if (m.brute)
            {
                for (size_t idx = 0; idx < m.tris.size(); ++idx)
                    m.test(tl, gl, l, idx, out);
                return;
            }
            double lu1, lu2;
            shape(tl, lu1, lu2);
            const double r = m.logF + kSlack;
            const int64_t b1lo = m.bin(lu1 - r), b1hi = m.bin(lu1 + r);
            const int64_t b2lo = m.bin(lu2 - r), b2hi = m.bin(lu2 + r);
            for (int64_t b1 = b1lo; b1 <= b1hi; ++b1)
                for (int64_t b2 = b2lo; b2 <= b2hi; ++b2)
                {
                    const int64_t key = cellKey(b1, b2);
                    auto range = std::equal_range(m.keys.begin(),
                                                  m.keys.end(), key);
                    for (auto it = range.first; it != range.second;
                         ++it)
                        m.test(tl, gl, l, size_t(it - m.keys.begin()),
                               out);
                }
        }
    }  // namespace detail

    template class ALIGNCALC_INSTANTIATION_API
        SimilarityPoints2D<LabelRules>;
    template ALIGNCALC_INSTANTIATION_API bool buildEdgesFirst(
        const SimilarityPoints2D<LabelRules>&, const BuildParams&,
        Progress&, GraphRows&);
    template ALIGNCALC_INSTANTIATION_API bool buildGraph(
        const SimilarityPoints2D<LabelRules>&, const BuildParams&,
        Progress&, GraphRows&);
}  // namespace AlignCalc
