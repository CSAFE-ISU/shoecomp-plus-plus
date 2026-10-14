#include "clq/clqmtch.h"
#include "graphBuild.h"
#include <algorithm>

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

        void finalizeRows(std::vector<Correspondence>&& nodes,
                          std::vector<std::vector<int32_t>>&& rows,
                          GraphRows& out)
        {
            const size_t n = nodes.size();
            std::vector<int32_t> remap(n, -1);
            int32_t kept = 0;
            for (size_t i = 0; i < n; ++i)
            {
                auto& row = rows[i];
                std::sort(row.begin(), row.end());
                row.erase(std::unique(row.begin(), row.end()),
                          row.end());
                if (!row.empty()) remap[i] = kept++;
            }

            out.nodes.clear();
            out.adj.clear();
            out.offsets.assign(1, 0);
            out.nodes.reserve(kept);
            out.offsets.reserve(kept + 1);
            for (size_t i = 0; i < n; ++i)
            {
                if (remap[i] < 0) continue;
                out.nodes.push_back(nodes[i]);
                for (int32_t j : rows[i])
                    if (remap[j] >= 0) out.adj.push_back(remap[j]);
                out.offsets.push_back(out.adj.size());
            }
        }

        void finalizeEdges(std::vector<Correspondence>&& nodes,
                           std::vector<EdgeAccumulator>& accs,
                           int32_t minSupport, GraphRows& out)
        {
            std::vector<std::pair<uint64_t, int32_t>> all;
            for (auto& acc : accs)
            {
                acc.flush();
                all.insert(all.end(), acc.counted_.begin(),
                           acc.counted_.end());
                acc.counted_.clear();
                acc.counted_.shrink_to_fit();
            }
            std::sort(all.begin(), all.end());

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
            finalizeRows(std::move(nodes), std::move(rows), out);
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
