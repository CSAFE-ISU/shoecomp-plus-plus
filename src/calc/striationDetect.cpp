#include "calc/striationDetect.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace shoecomp
{
    namespace
    {
        struct Lattice
        {
            int nTheta = 0;
            int nS = 0;
            double thetaMin = 0.0, thetaRange = 0.0;
            double sMin = 0.0, sRange = 0.0;
            std::vector<double> d;  // mean dr per bin; NaN if empty
        };

        int clampi(int v, int lo, int hi)
        {
            return v < lo ? lo : (v > hi ? hi : v);
        }

        // Light 3x3 mean over finite neighbours, into a fresh grid.
        void smooth3x3(const Lattice& in, std::vector<double>& out)
        {
            const int W = in.nTheta, H = in.nS;
            out.assign((size_t)W * H,
                       std::numeric_limits<double>::quiet_NaN());
            for (int t = 0; t < W; ++t)
                for (int s = 0; s < H; ++s)
                {
                    double sum = 0.0;
                    int cnt = 0;
                    for (int dt = -1; dt <= 1; ++dt)
                        for (int ds = -1; ds <= 1; ++ds)
                        {
                            int tt = t + dt, ss = s + ds;
                            if (tt < 0 || tt >= W || ss < 0 || ss >= H)
                                continue;
                            double v = in.d[(size_t)tt * H + ss];
                            if (!std::isfinite(v)) continue;
                            sum += v;
                            ++cnt;
                        }
                    if (cnt > 0) out[(size_t)t * H + s] = sum / cnt;
                }
        }
    }  // namespace

    bool detectStriations(
        const CylinderFit& fit, const Surface3D& surf,
        const StriationParams& params, std::vector<PeakRegion5D>& out,
        std::string& err, const std::atomic<bool>* cancel,
        const std::function<void(float, const char*)>& progress)
    {
        out.clear();
        auto tick = [&](float f, const char* t)
        {
            if (progress) progress(f, t);
        };
        auto stop = [&]() { return cancel && cancel->load(); };

        if (!fit.valid || fit.radius <= 0.0 || !surf.valid())
        {
            err =
                "cannot detect striations without a valid cylinder fit";
            return false;
        }

        const double thetaRange = fit.thetaMax - fit.thetaMin;
        const double sRange = fit.sMax - fit.sMin;
        if (thetaRange <= 0.0 || sRange <= 0.0)
        {
            err = "cylinder fit has a degenerate angular/axial range";
            return false;
        }

        // Lattice resolution: about one bin per physical grid step
        // along each direction (arc length for theta, metres for s),
        // clamped.
        double gridStep =
            std::min(std::abs(surf.incX), std::abs(surf.incY));
        if (!(gridStep > 0.0)) gridStep = 1.0;
        double arcLen = thetaRange * fit.radius;
        int nTheta =
            clampi((int)std::lround(arcLen / gridStep), 64, 1536);
        int nS = clampi((int)std::lround(sRange / gridStep), 64, 2048);

        Lattice lat;
        lat.nTheta = nTheta;
        lat.nS = nS;
        lat.thetaMin = fit.thetaMin;
        lat.thetaRange = thetaRange;
        lat.sMin = fit.sMin;
        lat.sRange = sRange;

        std::vector<double> sum((size_t)nTheta * nS, 0.0);
        std::vector<int> cnt((size_t)nTheta * nS, 0);

        tick(0.0f, "Unrolling surface...");
        const int Y = surf.sizeY, X = surf.sizeX;
        for (int r = 0; r < Y; ++r)
        {
            if (stop())
            {
                err = "cancelled";
                return false;
            }
            for (int c = 0; c < X; ++c)
            {
                float zv = surf.z(r, c);
                if (!std::isfinite(zv)) continue;
                Eigen::Vector3d p(surf.px(c), surf.py(r), surf.pz(zv));
                double th, s, rr;
                toCylindrical(fit, p, th, s, rr);
                double dr = rr - fit.radius;
                int it = clampi(
                    (int)((th - fit.thetaMin) / thetaRange * nTheta), 0,
                    nTheta - 1);
                int is = clampi((int)((s - fit.sMin) / sRange * nS), 0,
                                nS - 1);
                size_t idx = (size_t)it * nS + is;
                sum[idx] += dr;
                cnt[idx] += 1;
            }
            if ((r & 31) == 0)
                tick(0.7f * (float)(r + 1) / (float)Y,
                     "Unrolling surface...");
        }

        lat.d.assign((size_t)nTheta * nS,
                     std::numeric_limits<double>::quiet_NaN());
        for (size_t i = 0; i < lat.d.size(); ++i)
            if (cnt[i] > 0) lat.d[i] = sum[i] / cnt[i];

        std::vector<double> smoothed;
        smooth3x3(lat, smoothed);
        lat.d.swap(smoothed);

        if (stop())
        {
            err = "cancelled";
            return false;
        }
        tick(0.8f, "Finding striations...");

        // Connected components per sign (8-connectivity). label: 0 =
        // unlabelled, positive = component id. Ridge and groove masks
        // are scanned separately so opposite signs never merge.
        const int W = nTheta, H = nS;
        std::vector<int> label((size_t)W * H, 0);
        std::vector<std::pair<int, int>> stack;

        std::vector<PeakRegion5D> cands;
        const double binT = thetaRange / nTheta;
        const double binS = sRange / nS;

        for (int sign = -1; sign <= 1; sign += 2)
        {
            int nextId = 1;
            std::fill(label.begin(), label.end(), 0);
            for (int t0 = 0; t0 < W; ++t0)
                for (int s0 = 0; s0 < H; ++s0)
                {
                    size_t seed = (size_t)t0 * H + s0;
                    if (label[seed] != 0) continue;
                    double dv = lat.d[seed];
                    if (!std::isfinite(dv)) continue;
                    bool inMask = (sign > 0) ? (dv >= params.minDr)
                                             : (dv <= -params.minDr);
                    if (!inMask) continue;

                    int id = nextId++;
                    stack.clear();
                    stack.emplace_back(t0, s0);
                    label[seed] = id;
                    int tMinB = t0, tMaxB = t0, sMinB = s0, sMaxB = s0;
                    double peak = dv;  // signed extreme by |.|
                    while (!stack.empty())
                    {
                        auto [ct, cs] = stack.back();
                        stack.pop_back();
                        tMinB = std::min(tMinB, ct);
                        tMaxB = std::max(tMaxB, ct);
                        sMinB = std::min(sMinB, cs);
                        sMaxB = std::max(sMaxB, cs);
                        double cd = lat.d[(size_t)ct * H + cs];
                        if (std::abs(cd) > std::abs(peak)) peak = cd;
                        for (int dt = -1; dt <= 1; ++dt)
                            for (int ds = -1; ds <= 1; ++ds)
                            {
                                if (dt == 0 && ds == 0) continue;
                                int nt = ct + dt, ns = cs + ds;
                                if (nt < 0 || nt >= W || ns < 0 ||
                                    ns >= H)
                                    continue;
                                size_t ni = (size_t)nt * H + ns;
                                if (label[ni] != 0) continue;
                                double nv = lat.d[ni];
                                if (!std::isfinite(nv)) continue;
                                bool m = (sign > 0)
                                             ? (nv >= params.minDr)
                                             : (nv <= -params.minDr);
                                if (!m) continue;
                                label[ni] = id;
                                stack.emplace_back(nt, ns);
                            }
                    }

                    PeakRegion5D pr;
                    pr.thetaStart = fit.thetaMin + tMinB * binT;
                    pr.thetaEnd = fit.thetaMin + (tMaxB + 1) * binT;
                    pr.sStart = fit.sMin + sMinB * binS;
                    pr.length = (sMaxB + 1 - sMinB) * binS;
                    pr.dHeight = peak;
                    cands.push_back(pr);
                }
        }

        // Filter by angular width and axial length (|dHeight| already
        // satisfies minDr from the mask).
        std::vector<PeakRegion5D> kept;
        for (const auto& p : cands)
        {
            if (p.thetaEnd - p.thetaStart < params.minThetaDiff)
                continue;
            if (p.length < params.minLength) continue;
            kept.push_back(p);
        }

        std::sort(
            kept.begin(), kept.end(),
            [](const PeakRegion5D& a, const PeakRegion5D& b)
            { return std::abs(a.dHeight) > std::abs(b.dHeight); });
        if (params.maxCount >= 0 && (int)kept.size() > params.maxCount)
            kept.resize(params.maxCount);

        out = std::move(kept);
        tick(1.0f, "Done");
        return true;
    }
}  // namespace shoecomp
