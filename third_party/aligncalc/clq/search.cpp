#include "clq/clqmtch.h"
#include "graphBuild.h"
#include <algorithm>
#include <atomic>

namespace AlignCalc
{
    namespace detail
    {
        void EdgeAccumulator::flush()
        {
            if (pending_.empty()) return;
            std::sort(pending_.begin(), pending_.end());
            std::vector<std::pair<uint64_t, int32_t>> merged;
            merged.reserve(counted_.size() + pending_.size());
            size_t c = 0;
            for (size_t p = 0; p < pending_.size();)
            {
                uint64_t key = pending_[p];
                int32_t n = 0;
                for (; p < pending_.size() && pending_[p] == key; ++p)
                    ++n;
                for (; c < counted_.size() && counted_[c].first < key;
                     ++c)
                    merged.push_back(counted_[c]);
                if (c < counted_.size() && counted_[c].first == key)
                {
                    n += counted_[c].second;
                    ++c;
                }
                merged.push_back({key, n});
            }
            for (; c < counted_.size(); ++c)
                merged.push_back(counted_[c]);
            counted_.swap(merged);
            pending_.clear();
        }

        template <class Fn>
        static void parallelFor(size_t numWorkers, size_t total,
                                size_t block, Fn&& fn)
        {
            std::atomic<size_t> next{0};
            runWorkers(
                std::min(numWorkers, (total + block - 1) / block),
                [&](size_t)
                {
                    for (;;)
                    {
                        size_t b = next.fetch_add(block);
                        if (b >= total) return;
                        size_t e = std::min(b + block, total);
                        for (size_t i = b; i < e; ++i) fn(i);
                    }
                });
        }

        void finalizeRows(std::vector<Correspondence>&& nodes,
                          std::vector<std::vector<int32_t>>&& rows,
                          size_t numWorkers, GraphRows& out)
        {
            const size_t n = nodes.size();
            std::vector<int32_t> remap(n, -1);
            std::vector<int32_t> keptIdx;
            for (size_t i = 0; i < n; ++i)
                if (!rows[i].empty())
                {
                    remap[i] = static_cast<int32_t>(keptIdx.size());
                    keptIdx.push_back(static_cast<int32_t>(i));
                }
            const size_t kept = keptIdx.size();
            constexpr size_t kBlock = 256;

            out.offsets.assign(kept + 1, 0);
            parallelFor(numWorkers, kept, kBlock,
                        [&](size_t k)
                        {
                            uint64_t d = 0;
                            for (int32_t j : rows[keptIdx[k]])
                                d += remap[j] >= 0;
                            out.offsets[k + 1] = d;
                        });
            for (size_t k = 0; k < kept; ++k)
                out.offsets[k + 1] += out.offsets[k];

            out.nodes.resize(kept);
            out.adj.resize(out.offsets[kept]);
            parallelFor(numWorkers, kept, kBlock,
                        [&](size_t k)
                        {
                            auto& row = rows[keptIdx[k]];
                            int32_t* dst =
                                out.adj.data() + out.offsets[k];
                            for (int32_t j : row)
                                if (remap[j] >= 0) *dst++ = remap[j];
                            out.nodes[k] = nodes[keptIdx[k]];
                            std::vector<int32_t>().swap(row);
                        });
        }

        void finalizeEdges(std::vector<Correspondence>&& nodes,
                           std::vector<EdgeAccumulator>& accs,
                           int32_t minSupport, GraphRows& out)
        {
            using Run = std::vector<std::pair<uint64_t, int32_t>>;
            const size_t numWorkers = std::max<size_t>(1, accs.size());
            std::vector<Run> runs(accs.size());
            parallelFor(numWorkers, accs.size(), 1,
                        [&](size_t t)
                        {
                            accs[t].flush();
                            runs[t].swap(accs[t].counted_);
                        });
            while (runs.size() > 1)
            {
                std::vector<Run> merged((runs.size() + 1) / 2);
                parallelFor(numWorkers, merged.size(), 1,
                            [&](size_t m)
                            {
                                if (2 * m + 1 >= runs.size())
                                {
                                    merged[m].swap(runs[2 * m]);
                                    return;
                                }
                                Run& a = runs[2 * m];
                                Run& b = runs[2 * m + 1];
                                merged[m].resize(a.size() + b.size());
                                std::merge(a.begin(), a.end(),
                                           b.begin(), b.end(),
                                           merged[m].begin());
                                Run().swap(a);
                                Run().swap(b);
                            });
                runs.swap(merged);
            }
            Run all;
            if (!runs.empty()) all.swap(runs[0]);

            // rows come out sorted: keys are ordered by (a, b), a < b
            std::vector<std::vector<int32_t>> rows(nodes.size());
            for (size_t p = 0; p < all.size();)
            {
                uint64_t key = all[p].first;
                int64_t support = 0;
                for (; p < all.size() && all[p].first == key; ++p)
                    support += all[p].second;
                if (support < minSupport) continue;
                int32_t a = int32_t(key >> 32);
                int32_t b = int32_t(key & 0xffffffffu);
                rows[a].push_back(b);
                rows[b].push_back(a);
            }
            Run().swap(all);
            finalizeRows(std::move(nodes), std::move(rows), numWorkers,
                         out);
        }
    }  // namespace detail

    bool isSymmetric(const GraphRows& g)
    {
        for (size_t i = 0; i < g.size(); ++i)
        {
            const int32_t* row = g.row(i);
            for (size_t k = 0; k < g.degree(i); ++k)
            {
                int32_t j = row[k];
                const int32_t* other = g.row(j);
                if (!std::binary_search(other, other + g.degree(j),
                                        int32_t(i)))
                    return false;
            }
        }
        return true;
    }

    namespace clqmtch
    {
        bool Graph::initFromRows(WorkerChannel& channel,
                                 const GraphRows& rows)
        {
            if (rows.size() == 0 || rows.adj.empty() ||
                channel.should_cancel())
            {
                return false;
            }
            this->num_vertices = rows.size();
            this->vertices.resize(this->num_vertices);
            this->el_size = rows.adj.size();
            this->edge_list = std::make_unique<i32[]>(this->el_size);
            std::copy(rows.adj.begin(), rows.adj.end(),
                      this->edge_list.get());
            this->maxDegree = -1;
            for (u64 i = 0; i < this->num_vertices; ++i)
            {
                const i32 N = static_cast<i32>(rows.degree(i));
                const i32* row = rows.row(i);
                // first neighbor with a larger id (equal-degree
                // tie-break in StackDFS::processVertex)
                const i32 spos =
                    i32(std::upper_bound(row, row + N, i32(i)) - row);
                this->vertices[i].ind1 = rows.nodes[i].left;
                this->vertices[i].ind2 = rows.nodes[i].right;
                this->vertices[i].init(rows.offsets[i], N, spos);
                if (N > this->maxDegree) this->maxDegree = N;
            }
            this->maxDepth = this->maxDegree;
            return true;
        }
    }  // namespace clqmtch

    bool searchCliques(const GraphRows& rows,
                       const CliqueParams& params,
                       WorkerChannel& channel,
                       std::vector<std::vector<Correspondence>>& out)
    {
        clqmtch::Graph G;
        if (!G.initFromRows(channel, rows)) return false;
        clqmtch::StackDFS S(std::max<size_t>(1, params.numWorkers),
                            params.numResults, params.lowerBound,
                            params.upperBound);
        std::vector<std::unordered_set<i32>> cliques;
        if (!S.processGraph(channel, G, cliques)) return false;

        out.clear();
        for (const auto& clq : cliques)
        {
            if (clq.size() < size_t(params.lowerBound)) continue;
            std::vector<Correspondence> c;
            c.reserve(clq.size());
            for (i32 v : clq) c.push_back(rows.nodes[v]);
            std::sort(c.begin(), c.end());
            out.push_back(std::move(c));
        }
        std::sort(out.begin(), out.end(),
                  [](const auto& a, const auto& b)
                  {
                      if (a.size() != b.size())
                          return a.size() > b.size();
                      return a < b;
                  });
        out.erase(std::unique(out.begin(), out.end()), out.end());
        return true;
    }

}  // namespace AlignCalc
