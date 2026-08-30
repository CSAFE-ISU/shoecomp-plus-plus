#include "calc/striationDetect.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <vector>

#include "aligncalc/eigen/Eigen/Eigenvalues"

// See .claude/striation-hough-plan.md for the design. The pipeline is:
// (1) local high-pass of the height grid -> ridge/groove crest points
// (fit-independent prominence), (2) estimate the axial direction from
// the crest cloud (PCA), (3) iterative direction-constrained 3-D Hough
// (project crests onto the plane perpendicular to each candidate
// direction; the densest bin is a line; peel off its inliers and
// repeat), (4) per line PCA refit -> start/direction/length + filters.
namespace shoecomp
{
    namespace
    {
        const double kNaN = std::numeric_limits<double>::quiet_NaN();
        const double kDeg = 3.14159265358979323846 / 180.0;

        // Internal tuning knobs (not exposed in the dialog).
        const double kConeHalf = 12.0 * kDeg;  // direction search cone
        const double kAngStep = 2.5 * kDeg;    // cone sampling step
        const double kBinCells = 3.0;          // Hough bin size (cells)
        const double kVoteFrac = 0.4;          // votes per unit length
        const int kMinVotesFloor = 5;

        int clampi(int v, int lo, int hi)
        {
            return v < lo ? lo : (v > hi ? hi : v);
        }

        struct Crest
        {
            Eigen::Vector3d p;  // physical position (metres)
            Eigen::Vector3d n;  // unit surface normal
            double h;           // signed prominence (+ ridge, - groove)
        };

        struct HoughDir
        {
            Eigen::Vector3d b;   // unit direction
            Eigen::Vector3d e1;  // projection-plane basis
            Eigen::Vector3d e2;
        };

        // Separable NaN-aware box mean of a (H x W) float grid
        // (row-major index r*W+c), half-window |hw| cells, into |dst|.
        // Cells with no finite neighbour stay NaN.
        void boxMeanSeparable(const std::vector<float>& src, int W,
                              int H, int hw, std::vector<float>& dst)
        {
            const float NaN = (float)kNaN;
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

        // Bin key packing signed indices (|idx| < 32768) into one long.
        long binKey(long iu, long iv)
        {
            return (iu + 32768L) * 65536L + (iv + 32768L);
        }
    }  // namespace

    bool detectStriations(
        const Surface3D& surf, const StriationParams& params,
        std::vector<StriationLine>& out, std::string& err,
        const std::atomic<bool>* cancel,
        const std::function<void(float, const char*)>& progress)
    {
        out.clear();
        auto tick = [&](float f, const char* t)
        {
            if (progress) progress(f, t);
        };
        auto stop = [&]() { return cancel && cancel->load(); };

        if (!surf.valid())
        {
            err = "cannot detect striations on an empty surface";
            return false;
        }
        const int Y = surf.sizeY, X = surf.sizeX;
        if (Y < 4 || X < 4)
        {
            err = "surface too small for striation detection";
            return false;
        }

        // --- (1) local high-pass -----------------------------------
        tick(0.0f, "Filtering surface...");
        std::vector<float> zf((size_t)Y * X);
        for (int r = 0; r < Y; ++r)
            for (int c = 0; c < X; ++c)
                zf[(size_t)r * X + c] = surf.z(r, c);

        int hw = clampi((int)std::lround(std::min(X, Y) * 0.05), 4, 60);
        std::vector<float> wide;
        boxMeanSeparable(zf, X, Y, hw, wide);

        std::vector<float> hLoc((size_t)Y * X, (float)kNaN);
        for (size_t i = 0; i < hLoc.size(); ++i)
        {
            if (std::isfinite(zf[i]) && std::isfinite(wide[i]))
                hLoc[i] = zf[i] - wide[i];
        }

        // --- (2) crest feature points ------------------------------
        tick(0.25f, "Finding crest points...");
        const bool axialAlongRows =
            std::abs(Y * surf.incY) >= std::abs(X * surf.incX);
        const double minH = params.minHeight;

        auto normalAt = [&](int r, int c, Eigen::Vector3d& n) -> bool
        {
            float zl = surf.z(r, c - 1), zr = surf.z(r, c + 1);
            float zu = surf.z(r - 1, c), zd = surf.z(r + 1, c);
            if (!(std::isfinite(zl) && std::isfinite(zr) &&
                  std::isfinite(zu) && std::isfinite(zd)))
                return false;
            Eigen::Vector3d dc(surf.px(c + 1) - surf.px(c - 1), 0.0,
                               surf.pz(zr) - surf.pz(zl));
            Eigen::Vector3d dr(0.0, surf.py(r + 1) - surf.py(r - 1),
                               surf.pz(zd) - surf.pz(zu));
            n = dc.cross(dr);
            double nn = n.norm();
            if (nn < 1e-20) return false;
            n /= nn;
            return true;
        };

        std::vector<Crest> crests;
        auto tryCrest = [&](int r, int c, double v, double a, double b)
        {
            // v is hLoc(r,c); a,b the two cross-direction neighbours.
            bool isMax = (v > a && v >= b && v >= minH);
            bool isMin = (v < a && v <= b && v <= -minH);
            if (!isMax && !isMin) return;
            Eigen::Vector3d n;
            if (!normalAt(r, c, n)) return;
            Crest cr;
            cr.p = Eigen::Vector3d(surf.px(c), surf.py(r),
                                   surf.pz(surf.z(r, c)));
            cr.n = n;
            cr.h = v;
            crests.push_back(cr);
        };

        if (axialAlongRows)
        {
            // striations run along rows; scan each row across columns.
            for (int r = 1; r < Y - 1; ++r)
            {
                if (stop())
                {
                    err = "cancelled";
                    return false;
                }
                for (int c = 1; c < X - 1; ++c)
                {
                    double v = hLoc[(size_t)r * X + c];
                    double a = hLoc[(size_t)r * X + c - 1];
                    double b = hLoc[(size_t)r * X + c + 1];
                    if (std::isfinite(v) && std::isfinite(a) &&
                        std::isfinite(b))
                        tryCrest(r, c, v, a, b);
                }
            }
        }
        else
        {
            // striations run along columns; scan each column across
            // rows.
            for (int c = 1; c < X - 1; ++c)
            {
                if (stop())
                {
                    err = "cancelled";
                    return false;
                }
                for (int r = 1; r < Y - 1; ++r)
                {
                    double v = hLoc[(size_t)r * X + c];
                    double a = hLoc[(size_t)(r - 1) * X + c];
                    double b = hLoc[(size_t)(r + 1) * X + c];
                    if (std::isfinite(v) && std::isfinite(a) &&
                        std::isfinite(b))
                        tryCrest(r, c, v, a, b);
                }
            }
        }

        const int N = (int)crests.size();
        const double gridStep =
            std::min(std::abs(surf.incX), std::abs(surf.incY));
        const double axialStep =
            axialAlongRows ? std::abs(surf.incY) : std::abs(surf.incX);
        const double binSize = std::max(kBinCells * gridStep, 1e-9);
        int minVotes =
            std::max(kMinVotesFloor,
                     axialStep > 0.0
                         ? (int)std::lround(
                               kVoteFrac * params.minLength / axialStep)
                         : kMinVotesFloor);
        if (N < minVotes)
        {
            tick(1.0f, "Done");
            return true;  // nothing to detect, not an error
        }

        // --- (2b) axial direction estimate (PCA of crest cloud) ----
        Eigen::Vector3d mean = Eigen::Vector3d::Zero();
        for (const auto& cr : crests) mean += cr.p;
        mean /= (double)N;
        Eigen::Matrix3d cov = Eigen::Matrix3d::Zero();
        for (const auto& cr : crests)
        {
            Eigen::Vector3d d = cr.p - mean;
            cov += d * d.transpose();
        }
        Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> es(cov);
        Eigen::Vector3d aEst = es.eigenvectors().col(2).normalized();

        // --- (3) candidate directions in a cone around aEst --------
        Eigen::Vector3d t = (std::abs(aEst.x()) < 0.9)
                                ? Eigen::Vector3d::UnitX()
                                : Eigen::Vector3d::UnitY();
        Eigen::Vector3d u1 = (t - t.dot(aEst) * aEst).normalized();
        Eigen::Vector3d u2 = aEst.cross(u1).normalized();
        std::vector<HoughDir> dirs;
        for (double tu = -kConeHalf; tu <= kConeHalf + 1e-9;
             tu += kAngStep)
            for (double tv = -kConeHalf; tv <= kConeHalf + 1e-9;
                 tv += kAngStep)
            {
                HoughDir hd;
                hd.b = (aEst + std::tan(tu) * u1 + std::tan(tv) * u2)
                           .normalized();
                Eigen::Vector3d s = (std::abs(hd.b.x()) < 0.9)
                                        ? Eigen::Vector3d::UnitX()
                                        : Eigen::Vector3d::UnitY();
                hd.e1 = (s - s.dot(hd.b) * hd.b).normalized();
                hd.e2 = hd.b.cross(hd.e1).normalized();
                dirs.push_back(hd);
            }

        // --- (3b) iterative Hough peeling --------------------------
        tick(0.5f, "Detecting lines...");
        const Eigen::Vector3d p0 = mean;
        std::vector<char> alive((size_t)N, 1);
        std::vector<StriationLine> kept;
        std::vector<double> keptScore;
        const int maxIter =
            std::min(500, std::max(20, params.maxCount * 6));
        const double distThresh = 2.0 * binSize;

        for (int iter = 0; iter < maxIter; ++iter)
        {
            if (stop())
            {
                err = "cancelled";
                return false;
            }
            tick(0.5f + 0.45f * (float)iter / (float)maxIter,
                 "Detecting lines...");

            long bestCount = 0, bestKey = 0;
            int bestDir = -1;
            for (int di = 0; di < (int)dirs.size(); ++di)
            {
                const HoughDir& hd = dirs[di];
                std::unordered_map<long, int> hist;
                hist.reserve((size_t)N);
                for (int i = 0; i < N; ++i)
                {
                    if (!alive[i]) continue;
                    Eigen::Vector3d d = crests[i].p - p0;
                    long iu =
                        (long)std::llround(d.dot(hd.e1) / binSize);
                    long iv =
                        (long)std::llround(d.dot(hd.e2) / binSize);
                    int cnt = ++hist[binKey(iu, iv)];
                    if (cnt > bestCount)
                    {
                        bestCount = cnt;
                        bestDir = di;
                        bestKey = binKey(iu, iv);
                    }
                }
            }

            if (bestDir < 0 || bestCount < minVotes) break;

            // Gather inliers of the winning bin (+/-1 bin tolerance).
            const HoughDir& hd = dirs[bestDir];
            long keyIu = bestKey / 65536L - 32768L;
            long keyIv = bestKey % 65536L - 32768L;
            std::vector<int> gathered;
            for (int i = 0; i < N; ++i)
            {
                if (!alive[i]) continue;
                Eigen::Vector3d d = crests[i].p - p0;
                long iu = (long)std::llround(d.dot(hd.e1) / binSize);
                long iv = (long)std::llround(d.dot(hd.e2) / binSize);
                if (std::abs(iu - keyIu) <= 1 &&
                    std::abs(iv - keyIv) <= 1)
                    gathered.push_back(i);
            }
            for (int i : gathered) alive[i] = 0;  // consume (NMS)
            if ((int)gathered.size() < minVotes) continue;

            // PCA refit of the gathered crest positions.
            Eigen::Vector3d c0 = Eigen::Vector3d::Zero();
            for (int i : gathered) c0 += crests[i].p;
            c0 /= (double)gathered.size();
            Eigen::Matrix3d cv = Eigen::Matrix3d::Zero();
            for (int i : gathered)
            {
                Eigen::Vector3d d = crests[i].p - c0;
                cv += d * d.transpose();
            }
            Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> les(cv);
            Eigen::Vector3d dir =
                les.eigenvectors().col(2).normalized();
            if (dir.dot(hd.b) < 0.0) dir = -dir;

            // Keep inliers close to the fitted line; recompute stats.
            double t0 = std::numeric_limits<double>::max();
            double t1 = -std::numeric_limits<double>::max();
            double sumH = 0.0;
            Eigen::Vector3d sumN = Eigen::Vector3d::Zero();
            int nin = 0;
            for (int i : gathered)
            {
                Eigen::Vector3d q = crests[i].p - c0;
                double tproj = q.dot(dir);
                Eigen::Vector3d perp = q - tproj * dir;
                if (perp.norm() > distThresh) continue;
                t0 = std::min(t0, tproj);
                t1 = std::max(t1, tproj);
                sumH += std::abs(crests[i].h);
                sumN += crests[i].n;
                ++nin;
            }
            if (nin < minVotes) continue;

            double length = t1 - t0;
            double strength = sumH / nin;
            double surfDev = 0.0;
            double nn = sumN.norm();
            if (nn > 1e-20)
            {
                Eigen::Vector3d navg = sumN / nn;
                double dp = std::abs(dir.dot(navg));
                surfDev = std::asin(dp < 1.0 ? dp : 1.0);
            }

            if (length < params.minLength) continue;
            if (strength < params.minHeight) continue;
            if (params.maxSurfaceDev > 0.0 &&
                surfDev > params.maxSurfaceDev)
                continue;

            StriationLine ln;
            ln.start = c0 + t0 * dir;
            ln.dir = dir;
            ln.length = length;
            kept.push_back(ln);
            keptScore.push_back(strength * length);
            if (params.maxCount > 0 &&
                (int)kept.size() >= params.maxCount * 3)
                break;  // enough candidates to rank
        }

        // --- (4) rank and cap --------------------------------------
        std::vector<int> order(kept.size());
        for (int i = 0; i < (int)order.size(); ++i) order[i] = i;
        std::sort(order.begin(), order.end(), [&](int a, int b)
                  { return keptScore[a] > keptScore[b]; });
        if (params.maxCount >= 0 && (int)order.size() > params.maxCount)
            order.resize(params.maxCount);
        out.reserve(order.size());
        for (int i : order) out.push_back(kept[i]);

        tick(1.0f, "Done");
        return true;
    }
}  // namespace shoecomp
