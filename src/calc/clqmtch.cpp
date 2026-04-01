#include "calc/clqmtch.h"
#include <iostream>
#include <algorithm>

namespace shoecomp
{
    namespace clqmtch
    {
        using IntPair = Node;

        // returns the number of bits on in n
        static inline u64 bitcount(u64 n)
        {
            n = n - ((n >> 1) & 0x5555555555555555);
            n = (n & 0x3333333333333333) +
                ((n >> 2) & 0x3333333333333333);
            n = (n + (n >> 4)) & 0x0f0f0f0f0f0f0f0f;
            n = n + (n >> 8);
            n = n + (n >> 16);
            n = n + (n >> 32);
            return (n & 0x7f);
        }

        u64 GraphBits::count() const
        {
            u64 sum = 0;
            u64 dlen =
                ((this->maxBits & 0x3fu) != 0) + (this->maxBits >> 6);
            this->data[dlen - 1] &= this->padCover;
            for (u64 i = 0; i < dlen; i++)
                sum += bitcount(this->data[i]);
            return sum;
        }

        bool Graph::init(WorkerChannel& channel, GraphInfo& info)
        {
            if (info.nodemap.size() == 0 || info.edges.size() == 0 ||
                channel.should_cancel())
            {
                return false;
            }
            this->num_vertices = info.nodemap.size();
            this->vertices.resize(this->num_vertices);
            this->el_size = info.edges.size();
            this->edge_list = std::make_unique<i32[]>(this->el_size);
            this->maxDegree = -1;
            this->maxDepth = -1;
            //
            if (channel.should_cancel()) { return false; }
            std::vector<IntPair> mapped_edges(el_size);
            u64 ctr = 0;
            for (const auto& e : info.edges)
            {
                mapped_edges[ctr].ind1 = info.nodemap[e.v1];
                mapped_edges[ctr].ind2 = info.nodemap[e.v2];
                ++ctr;
            }
            std::sort(mapped_edges.begin(), mapped_edges.end());
            //
            if (channel.should_cancel()) { return false; }
            for (const auto& nn : info.nodemap)
            {
                vertices[nn.second].ind1 = nn.first.ind1;
                vertices[nn.second].ind2 = nn.first.ind2;
            }
            //
            for (i32 i = 0, offset = 0; i < this->num_vertices; ++i)
            {
                if (channel.should_cancel()) { return false; }
                i32 spos = 0;
                i32 j = 0;
                for (; offset + j < el_size &&
                       (mapped_edges[offset + j].ind1 == i);
                     ++j)
                {
                    if (mapped_edges[offset + j].ind2 > i) { spos = i; }
                    this->edge_list[offset + j] =
                        mapped_edges[offset + j].ind2;
                }
                this->vertices[i].init(offset, j, spos);
                offset += j;
                if (this->vertices[i].N > this->maxDegree)
                {
                    this->maxDegree = this->vertices[i].N;
                }
            }
            this->maxDepth = maxDegree;
            //
            return true;
        }

        static bool binarySearch(const i32* const arr, i32 beg, i32 end,
                                 i32 val)
        {
            i32 loc;
            if (arr[end] <= val) { return arr[end] == val; }
            else if (arr[beg] >= val) { return arr[beg] == val; }
            while (beg <= end)
            {
                loc = beg + ((end - beg) >> 1);
                if (arr[loc] == val)
                    return true;
                else if (arr[loc] < val)
                    beg = loc + 1;
                else
                    end = loc - 1;
            }
            return false;
        }

        void SearchBlob::init(u64 maxDegree, u64 maxDepth)
        {
            this->numBits = maxDegree;
            this->stride =
                ((this->numBits & 0x3fu) != 0) + (this->numBits >> 6);
            this->vstack = std::make_unique<i32[]>(maxDepth + 2);
            this->total = stride * (maxDepth + 2);
            this->raw = std::make_unique<u64[]>(total);
        }

        void SearchBlob::loadBitset(GraphBits& gb, i32 depth,
                                    i32 vertBits)
        {
            if (stride * depth >= total) { depth -= 1; }
            if (depth < 0) { depth = 0; }
            if (vertBits > numBits) { vertBits = numBits; }
            gb.refer_from(raw.get() + depth * stride, vertBits, false);
        }

        void CliqueResult::construct(i32 vid, const GraphBits& src)
        {
            this->vertexID = vid;
            u64 dlen =
                ((src.maxBits & 0x3fu) != 0) + (src.maxBits >> 6);
            this->neighbors = std::make_unique<u64[]>(dlen);
            GraphBits dst;
            dst.copy_from(src, neighbors.get());
        }

        bool StackDFS::processGraph(
            WorkerChannel& channel, const Graph& graph,
            std::vector<std::unordered_set<i32>>& results)
        {
            CliqueResult answer;
            this->max_clique_size.store(lowerBound,
                                        std::memory_order_relaxed);
            std::vector<std::thread> workers;
            //
            int i1 = 0;
            int N1 = graph.num_vertices;
            bool others_inited = false;
            size_t tid = 0;
            bool queue_full = false;
            //
            for (tid = 0; tid < numWorkers; ++tid)
            {
                searchBlobs[tid].init(graph.maxDegree, upperBound);
            }
            workers.reserve(this->numWorkers);
            //
            while (i1 < N1 && results.size() < numResults)
            {
                if (channel.should_cancel())
                {
                    i1 = 0;
                    break;
                }

                queue_full = false;
                // produce task
                while (i1 < N1 && !queue_full)
                {
                    if (!work_q.try_push(i1))
                    {
                        queue_full = true;
                        break;
                    }
                    else { ++i1; }
                }

                if (!others_inited)
                {
                    others_inited = true;
                    // initialize other threads
                    for (tid = 0; tid < numWorkers; ++tid)
                    {
                        workers.emplace_back(
                            [this, &channel, &graph, tid]
                            {
                                i32 vertexID;
                                while (this->work_q.pop(vertexID))
                                {
                                    this->processVertex(
                                        channel, tid, vertexID, graph);
                                }
                            });
                    }
                }
                // consume results
                while (result_q.pop(answer))
                {
                    this->update(graph, results, std::move(answer));
                }
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(100));
                channel.report((i1 * 1.0f) / N1, "Processing...");
            }
            work_q.stop();

            // ensure workers are not waiting to submit results
            while (result_q.pop(answer))
            {
                if (i1 >= N1)
                {
                    this->update(graph, results, std::move(answer));
                }
            }

            if (others_inited)
            {
                // wait for other threads to join
                for (auto& thr : workers) thr.join();
            }

            if (i1 < N1)
            {
                // cancelled?
                channel.report(0.0f, "Cancelled");
                return false;
            }

            // consume results
            while (result_q.pop(answer))
            {
                this->update(graph, results, std::move(answer));
            }
            return true;
        }

        void StackDFS::processVertex(WorkerChannel& channel, size_t tid,
                                     i32 vertexID, const Graph& G)
        {
            SearchBlob& blob = this->searchBlobs[tid];
            const Vertex& cur = G.vertices[vertexID];
            i32 clique_potential;
            i32 candidates_left;
            i32 clique_size;
            i32 depth;
            i32 v1, v2;
            i32 j, k;
            i32 local_max = 0;
            GraphBits& res = blob.res();
            GraphBits cand;
            GraphBits next_cand;
            CliqueResult message;
            //
            if (channel.should_cancel()) { return; }
            //
            blob.loadBitset(res, 0, cur.N);
            blob.vstack[0] = vertexID /* never used */;
            blob.loadBitset(cand, 1, cur.N);
            depth = 1;
            //
            //
            res.clear();
            cand.clear();
            clique_size = 1;
            clique_potential = 1;
            for (j = 0; j < cur.N; j++)
            {
                if (channel.should_cancel()) { return; }
                v1 = G.edge_list[cur.elo + j];
                if (G.vertices[v1].N <= cur.N && j < cur.spos) continue;
                if (G.vertices[v1].N < cur.N) continue;
                cand.set(j);
                clique_potential++;
            }
            if (clique_potential <
                this->max_clique_size.load(std::memory_order_relaxed))
            {
                return;
            }
            //
            while (depth > 0)
            {
                if (channel.should_cancel()) { return; }
                blob.loadBitset(cand, depth, cur.N);
                blob.loadBitset(next_cand, depth + 1, cur.N);
                //
                for (j = cand.next(0); j < cur.N; j = cand.next(j + 1))
                {
                    cand.reset(j);
                    v1 = G.edge_list[cur.elo + j];
                    const Vertex& vert = G.vertices[v1];

                    next_cand.clear();
                    for (k = cand.next(j + 1); k < cur.N;
                         k = cand.next(k + 1))
                    {
                        v2 = G.edge_list[cur.elo + k];
                        if (binarySearch(G.edge_list.get(), vert.elo,
                                         vert.elo + vert.N - 1, v2))
                        {
                            next_cand.set(k);
                        }
                    }
                    candidates_left = next_cand.count();
                    clique_potential =
                        candidates_left + 1 + clique_size;

                    if (clique_potential >= local_max &&
                        clique_potential >=
                            this->max_clique_size.load(
                                std::memory_order_relaxed))
                    {
                        if (candidates_left == 0 ||
                            clique_size + 1 >= upperBound)
                        {
                            local_max = 1 + clique_size;
                            res.set(j);
                            message.construct(vertexID, res);
                            result_q.push(std::move(message));
                            res.reset(j);
                        }
                        else if (candidates_left > 0)
                        {
                            blob.vstack[depth] = j;
                            res.set(blob.vstack[depth]);
                            clique_size += 1;
                            depth += 1;
                            break;
                        }
                    }
                }

                if (j >= cur.N)
                {
                    depth -= 1;
                    res.reset(blob.vstack[depth]);
                    clique_size -= 1;
                }
            }
        }

        void StackDFS::update(
            const Graph& graph,
            std::vector<std::unordered_set<i32>>& results,
            CliqueResult&& answer)
        {
            std::unordered_set<i32> clique;
            const Vertex& cur = graph.vertices[answer.vertexID];
            GraphBits bits(answer.neighbors.get(), cur.N, false);
            i32 clq_size = 1 + bits.count();
            i32 cur_max =
                this->max_clique_size.load(std::memory_order_relaxed);
            //
            if (cur_max > clq_size) { return; }
            if (cur_max < clq_size)
            {
                cur_max = clq_size;
                this->max_clique_size.store(clq_size,
                                            std::memory_order_relaxed);
            }
            //
            for (u64 i = 0; i < cur.N; ++i)
            {
                if (bits[i])
                {
                    clique.insert(graph.edge_list[cur.elo + i]);
                }
            }
            clique.insert(answer.vertexID);
            results.push_back(clique);
            //
            results.erase(
                std::remove_if(
                    results.begin(), results.end(),
                    [&clq_size](std::unordered_set<i32>& other)
                    { return other.size() < clq_size; }),
                results.end());
        }

    } /* namespace clqmtch */
} /* namespace shoecomp */
