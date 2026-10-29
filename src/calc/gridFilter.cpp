#include "calc/gridFilter.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace shoecomp
{
    void boxMeanSeparable(const std::vector<float>& src, int W, int H,
                          int hw, std::vector<float>& dst)
    {
        const float NaN = std::numeric_limits<float>::quiet_NaN();
        std::vector<float> tmp((size_t)W * H, NaN);
        std::vector<double> ps;
        std::vector<int> pc;

        ps.resize(W + 1);
        pc.resize(W + 1);
        for (int r = 0; r < H; ++r)
        {
            ps[0] = 0.0;
            pc[0] = 0;
            for (int c = 0; c < W; ++c)
            {
                double v = src[(size_t)r * W + c];
                bool fin = std::isfinite(v);
                ps[c + 1] = ps[c] + (fin ? v : 0.0);
                pc[c + 1] = pc[c] + (fin ? 1 : 0);
            }
            for (int c = 0; c < W; ++c)
            {
                int lo = std::max(0, c - hw);
                int hi = std::min(W - 1, c + hw);
                int cnt = pc[hi + 1] - pc[lo];
                if (cnt > 0)
                    tmp[(size_t)r * W + c] =
                        (float)((ps[hi + 1] - ps[lo]) / cnt);
            }
        }

        dst.assign((size_t)W * H, NaN);
        ps.resize(H + 1);
        pc.resize(H + 1);
        for (int c = 0; c < W; ++c)
        {
            ps[0] = 0.0;
            pc[0] = 0;
            for (int r = 0; r < H; ++r)
            {
                double v = tmp[(size_t)r * W + c];
                bool fin = std::isfinite(v);
                ps[r + 1] = ps[r] + (fin ? v : 0.0);
                pc[r + 1] = pc[r] + (fin ? 1 : 0);
            }
            for (int r = 0; r < H; ++r)
            {
                int lo = std::max(0, r - hw);
                int hi = std::min(H - 1, r + hw);
                int cnt = pc[hi + 1] - pc[lo];
                if (cnt > 0)
                    dst[(size_t)r * W + c] =
                        (float)((ps[hi + 1] - ps[lo]) / cnt);
            }
        }
    }
}  // namespace shoecomp
