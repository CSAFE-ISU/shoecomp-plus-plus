#include "calc/striationDetect.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <thread>
#include <unordered_map>
#include <vector>

#include "aligncalc/eigen/Eigen/Eigenvalues"

// See .claude/striation-hough-plan.md for the design. The pipeline is:
// (1) local high-pass of the height grid -> ridge/groove crest points
// (fit-independent prominence), (2) estimate the axial direction from
// the surface (gradient structure picks the along-striation grid axis;
// aEst is its mean physical tangent), (3) iterative
// direction-constrained 3-D Hough (project crests onto the plane
// perpendicular to each candidate direction; the densest bin is a line;
// peel off its inliers and repeat), (4) per line PCA refit ->
// start/direction/length + filters.
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

        // Ranking (see .claude/striation-hough-plan.md "Ranking"):
        // lines are ranked by inlier (crest vote) count, then
        // de-weighted by how close they sit to the land edges, so the
        // shallower central striations are not crowded out by the tall
        // edge shoulders. kEdgePenalty is the fraction of score an
        // outermost line loses; kEdgeFloor keeps every edge line some
        // weight.
        const double kEdgePenalty = 0.6;
        const double kEdgeFloor = 0.15;

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

        // A worker thread's best (densest) Hough bin over its slice of
        // the candidate directions. di = -1 means the slice was empty.
        struct DirBest
        {
            long count = 0;  // votes in the densest bin
            int di = -1;     // winning direction index
            long key = 0;    // packed bin key of that bin
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
        // Estimate striation orientation from the high-pass gradients:
        // variation is largest ACROSS striations, so the grid axis with
        // the larger squared gradient is the cross direction. (A land's
        // circumferential extent can exceed its axial extent, so the
        // longer grid dimension is NOT a reliable axial cue.)
        double sgx2 = 0.0, sgy2 = 0.0;
        for (int r = 1; r < Y - 1; ++r)
            for (int c = 1; c < X - 1; ++c)
            {
                double vl = hLoc[(size_t)r * X + c - 1];
                double vr = hLoc[(size_t)r * X + c + 1];
                double vu = hLoc[(size_t)(r - 1) * X + c];
                double vd = hLoc[(size_t)(r + 1) * X + c];
                if (std::isfinite(vl) && std::isfinite(vr))
                    sgx2 += 0.25 * (vr - vl) * (vr - vl);
                if (std::isfinite(vu) && std::isfinite(vd))
                    sgy2 += 0.25 * (vd - vu) * (vd - vu);
            }
        // Larger cross-column gradient => striations run along rows.
        const bool axialAlongRows = sgx2 > sgy2;
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

        // --- (2b) axial direction estimate -------------------------
        // Origin for the Hough projections: the crest centroid.
        Eigen::Vector3d mean = Eigen::Vector3d::Zero();
        for (const auto& cr : crests) mean += cr.p;
        mean /= (double)N;

        // The axial direction is the mean PHYSICAL tangent along the
        // chosen along-striation grid axis. A PCA of the crest cloud
        // would instead return the cross direction, because many
        // parallel striations spread the cloud widest across
        // themselves.
        Eigen::Vector3d aEst = Eigen::Vector3d::Zero();
        {
            int stride = std::max(1, (X * Y) / 200000);
            long k = 0;
            for (int r = 1; r < Y - 1; ++r)
                for (int c = 1; c < X - 1; ++c, ++k)
                {
                    if (k % stride) continue;
                    Eigen::Vector3d tng;
                    if (axialAlongRows)
                    {
                        float zu = surf.z(r - 1, c),
                              zd = surf.z(r + 1, c);
                        if (!std::isfinite(zu) || !std::isfinite(zd))
                            continue;
                        tng = Eigen::Vector3d(
                            0.0, surf.py(r + 1) - surf.py(r - 1),
                            surf.pz(zd) - surf.pz(zu));
                    }
                    else
                    {
                        float zl = surf.z(r, c - 1),
                              zr = surf.z(r, c + 1);
                        if (!std::isfinite(zl) || !std::isfinite(zr))
                            continue;
                        tng = Eigen::Vector3d(
                            surf.px(c + 1) - surf.px(c - 1), 0.0,
                            surf.pz(zr) - surf.pz(zl));
                    }
                    aEst += tng;
                }
        }
        if (aEst.norm() < 1e-20)
            aEst = axialAlongRows ? Eigen::Vector3d(0, 1, 0)
                                  : Eigen::Vector3d(1, 0, 0);
        aEst.normalize();

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

        // Direction-loop parallelism. Each iteration's search over the
        // candidate directions is independent per direction (every
        // direction builds its own histogram over the read-only crest
        // cloud), so split the directions across worker threads and
        // reduce their local bests. The reduction keeps the serial
        // tie-break -- highest vote count, then lowest direction index
        // -- so the outcome is identical to the single-threaded scan
        // for any thread count (important: detection stays
        // reproducible). Small inputs run serially; the thread setup
        // would not pay off.
        int nThreads =
            (int)std::max(1u, std::thread::hardware_concurrency());
        nThreads = std::min(nThreads, (int)dirs.size());
        if (nThreads < 1 || N < 512) nThreads = 1;
        std::vector<std::unordered_map<long, int>> histPool(nThreads);
        for (auto& h : histPool) h.reserve((size_t)N + 1);
        std::vector<DirBest> partial(nThreads);

        // Scan the direction slice assigned to worker |t| against the
        // current |alive| crests, recording that slice's densest bin.
        auto scanChunk = [&](int t)
        {
            const int D = (int)dirs.size();
            const int lo = (int)((long)t * D / nThreads);
            const int hi = (int)((long)(t + 1) * D / nThreads);
            std::unordered_map<long, int>& hist = histPool[t];
            DirBest best;
            for (int di = lo; di < hi; ++di)
            {
                const HoughDir& hd = dirs[di];
                hist.clear();
                long dCount = 0, dKey = 0;
                for (int i = 0; i < N; ++i)
                {
                    if (!alive[i]) continue;
                    Eigen::Vector3d d = crests[i].p - p0;
                    long iu =
                        (long)std::llround(d.dot(hd.e1) / binSize);
                    long iv =
                        (long)std::llround(d.dot(hd.e2) / binSize);
                    long k = binKey(iu, iv);
                    int cnt = ++hist[k];
                    if (cnt > dCount)
                    {
                        dCount = cnt;
                        dKey = k;
                    }
                }
                if (dCount > best.count)
                {
                    best.count = dCount;
                    best.di = di;
                    best.key = dKey;
                }
            }
            partial[t] = best;
        };

        for (int iter = 0; iter < maxIter; ++iter)
        {
            if (stop())
            {
                err = "cancelled";
                return false;
            }
            tick(0.5f + 0.45f * (float)iter / (float)maxIter,
                 "Detecting lines...");

            if (nThreads == 1) { scanChunk(0); }
            else
            {
                std::vector<std::thread> pool;
                pool.reserve(nThreads - 1);
                for (int t = 1; t < nThreads; ++t)
                    pool.emplace_back(scanChunk, t);
                scanChunk(0);
                for (std::thread& th : pool) th.join();
            }

            // Reduce the per-thread bests: highest vote count wins,
            // ties go to the lowest direction index (matching the
            // serial scan).
            long bestCount = 0, bestKey = 0;
            int bestDir = -1;
            for (int t = 0; t < nThreads; ++t)
            {
                const DirBest& b = partial[t];
                if (b.di < 0) continue;
                if (b.count > bestCount ||
                    (b.count == bestCount && b.di < bestDir))
                {
                    bestCount = b.count;
                    bestDir = b.di;
                    bestKey = b.key;
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
            // Rank on the inlier (crest vote) count: a clean,
            // continuous striation has a crest on nearly every scanline
            // it crosses, while a rough/broken one collects fewer.
            // strength/length still gate the line above via the
            // filters.
            keptScore.push_back((double)nin);
        }

        // --- (3c) merge near-duplicate lines -----------------------
        // The iterative Hough can re-detect one striation from adjacent
        // projection bins across neighbouring candidate directions.
        // Collapse lines that are near-parallel and laterally close
        // (the same ruling on the cylinder) into one, taking the union
        // of their extents, strongest first.
        {
            std::vector<int> order(kept.size());
            for (int i = 0; i < (int)order.size(); ++i) order[i] = i;
            std::sort(order.begin(), order.end(), [&](int a, int b)
                      { return keptScore[a] > keptScore[b]; });
            const double cosTol = std::cos(8.0 * kDeg);
            const double perpTol = kBinCells * gridStep;
            std::vector<StriationLine> merged;
            std::vector<double> mergedScore;
            for (int idx : order)
            {
                const StriationLine& a = kept[idx];
                Eigen::Vector3d amid = a.start + 0.5 * a.length * a.dir;
                int hit = -1;
                for (int m = 0; m < (int)merged.size(); ++m)
                {
                    const StriationLine& b = merged[m];
                    if (std::abs(a.dir.dot(b.dir)) < cosTol) continue;
                    Eigen::Vector3d q = amid - b.start;
                    double t = q.dot(b.dir);
                    if ((q - t * b.dir).norm() <= perpTol)
                    {
                        hit = m;
                        break;
                    }
                }
                if (hit < 0)
                {
                    merged.push_back(a);
                    mergedScore.push_back(keptScore[idx]);
                    continue;
                }
                // Extend the accepted line to cover a's extent.
                StriationLine& b = merged[hit];
                double ta0 = (a.start - b.start).dot(b.dir);
                double ta1 =
                    (a.start + a.length * a.dir - b.start).dot(b.dir);
                double lo = std::min({0.0, ta0, ta1});
                double hi = std::max({b.length, ta0, ta1});
                b.start = b.start + lo * b.dir;
                b.length = hi - lo;
            }
            kept.swap(merged);
            keptScore.swap(mergedScore);
        }

        // --- (4) rank and cap --------------------------------------
        // Base score is the inlier count (keptScore); multiply by an
        // edge de-weight so striations near the land edges rank lower.
        // The cross-position is the component of each line's midpoint
        // along the across-striation grid axis (px depends only on the
        // column, py only on the row, so this is simply the X or Y
        // coordinate); e in [0,1] is |cross| normalized by the widest
        // line, and the weight falls off as e^2.
        const Eigen::Vector3d cEst =
            axialAlongRows ? Eigen::Vector3d(1.0, 0.0, 0.0)
                           : Eigen::Vector3d(0.0, 1.0, 0.0);
        std::vector<double> cpos(kept.size(), 0.0);
        double cmax = 0.0;
        for (int i = 0; i < (int)kept.size(); ++i)
        {
            Eigen::Vector3d mid =
                kept[i].start + 0.5 * kept[i].length * kept[i].dir;
            cpos[i] = (mid - mean).dot(cEst);
            cmax = std::max(cmax, std::abs(cpos[i]));
        }
        std::vector<double> rankScore(kept.size(), 0.0);
        for (int i = 0; i < (int)kept.size(); ++i)
        {
            double e = cmax > 1e-12 ? std::abs(cpos[i]) / cmax : 0.0;
            double w = 1.0 - kEdgePenalty * e * e;
            if (w < kEdgeFloor) w = kEdgeFloor;
            rankScore[i] = keptScore[i] * w;
        }

        std::vector<int> order(kept.size());
        for (int i = 0; i < (int)order.size(); ++i) order[i] = i;
        std::sort(order.begin(), order.end(), [&](int a, int b)
                  { return rankScore[a] > rankScore[b]; });
        if (params.maxCount >= 0 && (int)order.size() > params.maxCount)
            order.resize(params.maxCount);
        out.reserve(order.size());
        for (int i : order) out.push_back(kept[i]);

        tick(1.0f, "Done");
        return true;
    }
}  // namespace shoecomp
