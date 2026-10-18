#include "aligncalc/correspondence.h"
#include "aligncalc/graphBuild.h"
#include <algorithm>
#include <atomic>
#include <limits>
#include <mutex>
#include <set>

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace AlignCalc
{
    namespace
    {
        using Word = uint64_t;

        inline int lowestBit(Word w)
        {
#ifdef _MSC_VER
            unsigned long i;
            _BitScanForward64(&i, w);
            return int(i);
#else
            return __builtin_ctzll(w);
#endif
        }

        // Batagelj-Zaversnik: |order| lists vertices in removal order
        // (smallest-last), |core| their core numbers.
        void coreOrder(const GraphRows& g, std::vector<int32_t>& order,
                       std::vector<int32_t>& core)
        {
            const int32_t n = int32_t(g.size());
            int32_t maxDeg = 0;
            core.resize(n);
            for (int32_t v = 0; v < n; ++v)
            {
                core[v] = int32_t(g.degree(v));
                maxDeg = std::max(maxDeg, core[v]);
            }
            std::vector<int32_t> bin(maxDeg + 2, 0), pos(n);
            order.resize(n);
            for (int32_t v = 0; v < n; ++v) ++bin[core[v]];
            for (int32_t d = 0, start = 0; d <= maxDeg; ++d)
            {
                int32_t c = bin[d];
                bin[d] = start;
                start += c;
            }
            for (int32_t v = 0; v < n; ++v)
            {
                pos[v] = bin[core[v]]++;
                order[pos[v]] = v;
            }
            for (int32_t d = maxDeg; d > 0; --d) bin[d] = bin[d - 1];
            bin[0] = 0;
            for (int32_t i = 0; i < n; ++i)
            {
                const int32_t v = order[i];
                const int32_t* row = g.row(v);
                for (size_t k = 0; k < g.degree(v); ++k)
                {
                    const int32_t u = row[k];
                    if (core[u] > core[v])
                    {
                        const int32_t du = core[u];
                        const int32_t pu = pos[u];
                        const int32_t pw = bin[du];
                        const int32_t w = order[pw];
                        if (u != w)
                        {
                            pos[u] = pw;
                            order[pu] = w;
                            pos[w] = pu;
                            order[pw] = u;
                        }
                        ++bin[du];
                        --core[u];
                    }
                }
            }
        }

        bool adjacent(const GraphRows& g, int32_t a, int32_t b)
        {
            const int32_t* row = g.row(a);
            return std::binary_search(row, row + g.degree(a), b);
        }

        // Branch and bound over each root's later neighbors with a
        // greedy-coloring bound (MCQ/BBMC style). Prunes only when the
        // bound is below the incumbent, so every maximum clique is
        // reached and the result set does not depend on thread timing
        // -- up to the tie cap; past it, searchCliques re-searches on
        // one thread (see capped()).
        class Solver
        {
           public:
            static constexpr size_t kTieCap = 4096;

            // |fixedSize| > 0: deterministic mode for that exact clique
            // size (see searchCliques): every root keeps the first
            // clique its search finds, and run() returns those of the
            // |tieCap| lowest roots that have one, in root order.
            Solver(const GraphRows& g, const CliqueParams& p,
                   Progress& progress, size_t tieCap = kTieCap,
                   int32_t fixedSize = 0)
                : g_(g), p_(p), progress_(progress), tieCap_(tieCap),
                  fixed_(fixedSize)
            {
            }

            bool run(std::vector<std::vector<int32_t>>& out);
            // More maximum cliques existed than the tie cap kept:
            // which ones were kept depends on thread timing.
            bool capped() const { return strict_.load(); }

           private:
            struct Scratch
            {
                std::vector<int32_t> local;
                std::vector<int32_t> members;
                std::vector<Word> adj;
                std::vector<Word> stack;
                std::vector<Word> q, qc;
                std::vector<std::vector<int32_t>> ord, col;
                std::vector<int32_t> clique;
                std::vector<int32_t> later;
                std::vector<Correspondence> cands;
                std::vector<uint8_t> keep;
                size_t words = 0;
                uint32_t ticks = 0;
                int32_t rootIdx = 0;  // index in root order
                bool hit = false;     // fixed mode: root has its clique
            };

            int32_t threshold() const
            {
                return best_.load(std::memory_order_relaxed) +
                       (strict_.load(std::memory_order_relaxed) ? 1
                                                                : 0);
            }
            int32_t greedy() const;
            void root(int32_t v, Scratch& s);
            void anchoredRoot(int32_t v, Scratch& s);
            void branch(Scratch& s);
            void expand(size_t depth, Scratch& s);
            void record(Scratch& s);
            // stop the current root: cancelled, or (fixed mode) done or
            // past the cutoff
            bool quit(const Scratch& s) const
            {
                return stop_.load(std::memory_order_relaxed) || s.hit ||
                       s.rootIdx >
                           cutoff_.load(std::memory_order_relaxed);
            }

            const GraphRows& g_;
            const CliqueParams& p_;
            Progress& progress_;
            const size_t tieCap_;
            const int32_t fixed_;
            std::vector<int32_t> order_, pos_, core_;
            std::atomic<int32_t> best_{0};
            std::atomic<bool> strict_{false};
            std::atomic<bool> stop_{false};
            // tie cap reached at upperBound: nothing left to find
            std::atomic<bool> complete_{false};
            std::mutex mu_;
            std::vector<std::vector<int32_t>> found_;
            // fixed mode: first clique per root, the roots that have
            // one, and the tieCap-th lowest of them (roots past it can
            // be skipped)
            std::vector<std::vector<int32_t>> rootHit_;
            std::set<int32_t> hitRoots_;
            std::atomic<int32_t> cutoff_{
                std::numeric_limits<int32_t>::max()};
        };

        int32_t Solver::greedy() const
        {
            const size_t n = g_.size();
            std::vector<int32_t> byCore(order_.rbegin(), order_.rend());
            const size_t tries = std::min<size_t>(16, n);
            int32_t bestSize = 0;
            std::vector<int32_t> cand, clique;
            for (size_t t = 0; t < tries; ++t)
            {
                const int32_t v = byCore[t];
                const int32_t* row = g_.row(v);
                cand.assign(row, row + g_.degree(v));
                std::sort(cand.begin(), cand.end(),
                          [&](int32_t a, int32_t b) {
                              return core_[a] != core_[b]
                                         ? core_[a] > core_[b]
                                         : a < b;
                          });
                clique.assign(1, v);
                for (int32_t w : cand)
                {
                    bool ok = true;
                    for (int32_t c : clique)
                        if (!adjacent(g_, w, c))
                        {
                            ok = false;
                            break;
                        }
                    if (ok) clique.push_back(w);
                }
                bestSize = std::max(bestSize, int32_t(clique.size()));
            }
            return bestSize;
        }

        void Solver::record(Scratch& sc)
        {
            const std::vector<int32_t>& clique = sc.clique;
            if (fixed_ > 0)
            {
                if (int32_t(clique.size()) < fixed_ || sc.hit) return;
                sc.hit = true;
                rootHit_[sc.rootIdx] = clique;
                std::lock_guard<std::mutex> lock(mu_);
                hitRoots_.insert(sc.rootIdx);
                if (hitRoots_.size() >= tieCap_)
                    cutoff_.store(
                        *std::next(hitRoots_.begin(), tieCap_ - 1));
                return;
            }
            const int32_t s = int32_t(clique.size());
            if (s < threshold()) return;
            std::lock_guard<std::mutex> lock(mu_);
            const int32_t b = best_.load();
            if (s > b)
            {
                best_.store(s);
                strict_.store(false);
                found_.clear();
                found_.push_back(clique);
            }
            else if (s == b && !strict_.load())
            {
                if (found_.size() < tieCap_)
                    found_.push_back(clique);
                else
                    strict_.store(true);
                // No larger clique can exist, so a full tie set ends
                // the search.
                if (found_.size() >= tieCap_ && b >= p_.upperBound)
                {
                    complete_.store(true);
                    stop_.store(true);
                }
            }
        }

        void Solver::root(int32_t v, Scratch& s)
        {
            int32_t thr = threshold();
            if (core_[v] + 1 < thr) return;
            s.members.clear();
            const int32_t* row = g_.row(v);
            for (size_t k = 0; k < g_.degree(v); ++k)
            {
                const int32_t w = row[k];
                if (pos_[w] > pos_[v] && core_[w] + 1 >= thr)
                    s.members.push_back(w);
            }
            if (int32_t(s.members.size()) + 1 < thr) return;
            s.clique.assign(1, v);
            branch(s);
        }

        // Anchored search: every clique is anchored on its two lowest
        // vertices in search order (v, v2), and the constraint picks
        // v2's candidates among v's later neighbors before any bitset
        // is built.
        void Solver::anchoredRoot(int32_t v, Scratch& s)
        {
            int32_t thr = threshold();
            if (core_[v] + 1 < thr) return;
            auto& later = s.later;
            later.clear();
            const int32_t* row = g_.row(v);
            for (size_t k = 0; k < g_.degree(v); ++k)
            {
                const int32_t w = row[k];
                if (pos_[w] > pos_[v] && core_[w] + 1 >= thr)
                    later.push_back(w);
            }
            if (later.empty() || p_.upperBound <= 1)
            {
                s.clique.assign(1, v);
                record(s);
                return;
            }
            if (int32_t(later.size()) + 1 < thr) return;
            std::sort(later.begin(), later.end(),
                      [&](int32_t a, int32_t b)
                      { return pos_[a] < pos_[b]; });
            s.cands.resize(later.size());
            for (size_t i = 0; i < later.size(); ++i)
                s.cands[i] = g_.nodes[later[i]];
            s.keep.resize(later.size());

            for (size_t i = 0; i < later.size(); ++i)
            {
                if (((++s.ticks) & 0xff) == 0 && progress_.cancelled())
                    stop_.store(true);
                if (quit(s)) return;
                thr = threshold();
                const size_t rest = later.size() - i - 1;
                if (int32_t(rest) + 2 < thr) return;
                const int32_t v2 = later[i];
                if (core_[v2] + 1 < thr) continue;
                p_.constraint->filter(g_.nodes[v], g_.nodes[v2],
                                      s.cands.data() + i + 1, rest,
                                      s.keep.data());
                s.members.clear();
                const int32_t* r2 = g_.row(v2);
                const int32_t* e2 = r2 + g_.degree(v2);
                for (size_t j = 0; j < rest; ++j)
                {
                    const int32_t w = later[i + 1 + j];
                    if (s.keep[j] && core_[w] + 1 >= thr &&
                        std::binary_search(r2, e2, w))
                        s.members.push_back(w);
                }
                if (int32_t(s.members.size()) + 2 < thr) continue;
                s.clique.assign({v, v2});
                branch(s);
            }
        }

        // Branch and bound over s.members, all adjacent to s.clique.
        void Solver::branch(Scratch& s)
        {
            // higher core first: a better greedy coloring
            std::sort(s.members.begin(), s.members.end(),
                      [&](int32_t a, int32_t b)
                      { return pos_[a] > pos_[b]; });

            const size_t m = s.members.size();
            const size_t W = (m + 63) / 64;
            s.words = W;
            s.adj.assign(m * W, 0);
            for (size_t a = 0; a < m; ++a)
                s.local[s.members[a]] = int32_t(a);
            for (size_t a = 0; a < m; ++a)
            {
                const int32_t u = s.members[a];
                const int32_t* r = g_.row(u);
                for (size_t k = 0; k < g_.degree(u); ++k)
                {
                    const int32_t b = s.local[r[k]];
                    if (b >= 0)
                        s.adj[a * W + b / 64] |= Word(1) << (b % 64);
                }
            }
            for (int32_t u : s.members) s.local[u] = -1;

            const size_t depths = m + 2;
            s.stack.assign(depths * W, 0);
            s.q.assign(W, 0);
            s.qc.assign(W, 0);
            if (s.ord.size() < depths)
            {
                s.ord.resize(depths);
                s.col.resize(depths);
            }
            for (size_t a = 0; a < m; ++a)
                s.stack[a / 64] |= Word(1) << (a % 64);
            if (m == 0 || int32_t(s.clique.size()) >= p_.upperBound)
                record(s);
            else
                expand(0, s);
        }

        void Solver::expand(size_t depth, Scratch& s)
        {
            const size_t W = s.words;
            Word* P = s.stack.data() + depth * W;
            const int32_t csize = int32_t(s.clique.size());
            auto& ord = s.ord[depth];
            auto& col = s.col[depth];
            ord.clear();
            col.clear();

            const int32_t kmin = threshold() - csize;
            std::copy(P, P + W, s.q.begin());
            int32_t color = 0;
            for (;;)
            {
                size_t first = 0;
                while (first < W && s.q[first] == 0) ++first;
                if (first == W) break;
                ++color;
                std::copy(s.q.begin(), s.q.end(), s.qc.begin());
                for (size_t w = first; w < W;)
                {
                    if (s.qc[w] == 0)
                    {
                        ++w;
                        continue;
                    }
                    const int32_t v =
                        int32_t(w * 64 + lowestBit(s.qc[w]));
                    const Word bit = Word(1) << (v % 64);
                    s.q[w] &= ~bit;
                    s.qc[w] &= ~bit;
                    const Word* nv = s.adj.data() + size_t(v) * W;
                    for (size_t x = w; x < W; ++x) s.qc[x] &= ~nv[x];
                    if (color >= kmin)
                    {
                        ord.push_back(v);
                        col.push_back(color);
                    }
                }
            }

            Word* next = P + W;
            for (size_t i = ord.size(); i-- > 0;)
            {
                if (((++s.ticks) & 0xfff) == 0 && progress_.cancelled())
                    stop_.store(true);
                if (quit(s)) return;
                if (csize + col[i] < threshold()) return;
                const int32_t v = ord[i];
                const Word* nv = s.adj.data() + size_t(v) * W;
                bool any = false;
                for (size_t x = 0; x < W; ++x)
                {
                    next[x] = P[x] & nv[x];
                    any |= next[x] != 0;
                }
                s.clique.push_back(s.members[v]);
                if (!any || int32_t(s.clique.size()) >= p_.upperBound)
                    record(s);
                else
                    expand(depth + 1, s);
                s.clique.pop_back();
                P[v / 64] &= ~(Word(1) << (v % 64));
            }
        }

        bool Solver::run(std::vector<std::vector<int32_t>>& out)
        {
            const int32_t n = int32_t(g_.size());
            if (fixed_ > 0) rootHit_.assign(n, {});
            coreOrder(g_, order_, core_);
            pos_.resize(n);
            for (int32_t i = 0; i < n; ++i) pos_[order_[i]] = i;
            // With a constraint, greedy's clique may be anchored on a
            // pair the search never uses as anchor, so its size is not
            // a safe starting bound.
            best_.store(std::max<int32_t>(
                std::max<int32_t>(p_.lowerBound, 1),
                p_.constraint
                    ? 0
                    : std::min<int32_t>(greedy(), p_.upperBound)));

            // roots from the dense end: big cliques early
            std::atomic<int32_t> next{0};
            std::atomic<int32_t> done{0};
            int lastPct = -1;
            detail::runWorkers(
                std::max<size_t>(1, p_.numWorkers),
                [&](size_t tid)
                {
                    Scratch s;
                    s.local.assign(n, -1);
                    for (;;)
                    {
                        if (stop_.load() || progress_.cancelled())
                        {
                            stop_.store(true);
                            return;
                        }
                        const int32_t i = next.fetch_add(1);
                        if (i >= n || i > cutoff_.load()) return;
                        const int32_t v = order_[n - 1 - i];
                        s.rootIdx = i;
                        s.hit = false;
                        if (p_.constraint)
                            anchoredRoot(v, s);
                        else
                            root(v, s);
                        const int32_t d = done.fetch_add(1) + 1;
                        if (tid == 0)
                        {
                            // 5% steps (see reportProgress in
                            // graphBuild.h).
                            int pct = int((100LL * d) / n) / 5 * 5;
                            if (pct != lastPct)
                            {
                                lastPct = pct;
                                progress_.report(
                                    0.5f + 0.0025f * pct,
                                    "searching cliques...");
                            }
                        }
                    }
                });
            if (stop_.load() && !complete_.load()) return false;
            if (fixed_ > 0)
            {
                out.clear();
                for (int32_t r : hitRoots_)
                {
                    if (out.size() >= tieCap_) break;
                    out.push_back(rootHit_[r]);
                }
                return true;
            }
            out.swap(found_);
            return true;
        }
    }  // namespace

    bool searchCliques(const GraphRows& rows,
                       const CliqueParams& params, Progress& progress,
                       std::vector<std::vector<Correspondence>>& out)
    {
        out.clear();
        if (rows.size() == 0 || rows.adj.empty() ||
            progress.cancelled())
            return false;
        const size_t want = std::max<size_t>(1, params.numResults);
        std::vector<std::vector<int32_t>> cliques;
        if (!params.deterministic)
        {
            Solver solver(rows, params, progress);
            if (!solver.run(cliques) || cliques.empty()) return false;
        }
        else
        {
            // The maximum size is always reproducible; which of many
            // tied cliques the parallel search keeps is not. So first
            // find the size (stop collecting ties at the second), then,
            // if it is tied, take the first clique of that size in
            // each root's (deterministic) search, for the lowest roots.
            // A clique at upperBound ends the first phase at once, so
            // it is whichever one a thread reached first: settle it
            // too.
            Solver solver(rows, params, progress, 1);
            if (!solver.run(cliques) || cliques.empty()) return false;
            if (solver.capped() ||
                int32_t(cliques.front().size()) >= params.upperBound)
            {
                CliqueParams fixed = params;
                fixed.lowerBound = fixed.upperBound =
                    int32_t(cliques.front().size());
                // the first pass has reported this stage already
                detail::CancelOnly quiet(progress);
                Solver again(rows, fixed, quiet, want,
                             fixed.lowerBound);
                cliques.clear();
                if (!again.run(cliques) || cliques.empty())
                    return false;
            }
        }

        for (const auto& clq : cliques)
        {
            std::vector<Correspondence> c;
            c.reserve(clq.size());
            for (int32_t v : clq) c.push_back(rows.nodes[v]);
            std::sort(c.begin(), c.end());
            out.push_back(std::move(c));
        }
        std::sort(out.begin(), out.end());
        out.erase(std::unique(out.begin(), out.end()), out.end());
        if (out.size() > want) out.resize(want);
        return true;
    }

}  // namespace AlignCalc
