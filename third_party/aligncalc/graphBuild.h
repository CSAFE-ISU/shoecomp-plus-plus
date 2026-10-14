#ifndef ALIGNCALC_GRAPHBUILD_H
#define ALIGNCALC_GRAPHBUILD_H
#include "correspondence.h"
#include <algorithm>
#include <atomic>
#include <thread>
#include <type_traits>
#include <utility>

namespace AlignCalc
{
    // Model contract (plain member functions, no virtual dispatch):
    //   int32_t leftCount() const;
    //   int32_t rightCount() const;
    //   bool admissible(int32_t left, int32_t right) const;
    // plus at least one generator:
    //   nodes-first:
    //     void consistentWith(Correspondence a,
    //                         std::vector<Correspondence>& out) const;
    //   edges-first, with static constexpr int arity = 2 or 3:
    //     void consistentWith(const std::array<int32_t, arity>& left,
    //                         std::vector<TupleMatch<arity>>& out)
    //         const;
    namespace detail
    {
        template <class M, class = void>
        struct HasBasics : std::false_type
        {
        };
        template <class M>
        struct HasBasics<
            M,
            std::void_t<decltype(std::declval<const M&>().leftCount()),
                        decltype(std::declval<const M&>().rightCount()),
                        decltype(std::declval<const M&>().admissible(
                            int32_t{}, int32_t{}))>> : std::true_type
        {
        };

        template <class M, class = void>
        struct HasNodeGen : std::false_type
        {
        };
        template <class M>
        struct HasNodeGen<
            M, std::void_t<
                   decltype(std::declval<const M&>().consistentWith(
                       std::declval<Correspondence>(),
                       std::declval<std::vector<Correspondence>&>()))>>
            : std::true_type
        {
        };

        template <class M, class = void>
        struct HasTupleGen : std::false_type
        {
        };
        template <class M>
        struct HasTupleGen<
            M, std::void_t<
                   decltype(M::arity),
                   decltype(std::declval<const M&>().consistentWith(
                       std::declval<
                           const std::array<int32_t, M::arity>&>(),
                       std::declval<
                           std::vector<TupleMatch<M::arity>>&>()))>>
            : std::true_type
        {
        };

        class EdgeAccumulator
        {
            std::vector<uint64_t> pending_;
            std::vector<std::pair<uint64_t, int32_t>> counted_;
            friend void finalizeEdges(std::vector<Correspondence>&&,
                                      std::vector<EdgeAccumulator>&,
                                      int32_t, GraphRows&);

           public:
            static constexpr size_t kFlushSize = size_t(1) << 20;
            void add(int32_t a, int32_t b)
            {
                if (a > b) std::swap(a, b);
                pending_.push_back((uint64_t(uint32_t(a)) << 32) |
                                   uint32_t(b));
                if (pending_.size() >= kFlushSize) flush();
            }
            void flush();
        };

        // |rows| must be sorted and duplicate-free.
        void finalizeRows(std::vector<Correspondence>&& nodes,
                          std::vector<std::vector<int32_t>>&& rows,
                          size_t numWorkers, GraphRows& out);
        void finalizeEdges(std::vector<Correspondence>&& nodes,
                           std::vector<EdgeAccumulator>& accs,
                           int32_t minSupport, GraphRows& out);

        inline void reportProgress(WorkerChannel& channel, size_t done,
                                   size_t total, int& lastPct)
        {
            int pct = total ? int((100 * done) / total) : 100;
            if (pct == lastPct) return;
            lastPct = pct;
            channel.report(0.25f + 0.0025f * pct, "building graph...");
        }

        // fn(tid) runs on numWorkers threads; tid 0 is the calling
        // thread, the only one allowed to post to the channel.
        template <class Fn>
        void runWorkers(size_t numWorkers, Fn&& fn)
        {
            if (numWorkers < 1) numWorkers = 1;
            std::vector<std::thread> threads;
            threads.reserve(numWorkers - 1);
            for (size_t tid = 1; tid < numWorkers; ++tid)
                threads.emplace_back([&fn, tid] { fn(tid); });
            fn(0);
            for (auto& t : threads) t.join();
        }

        template <class Model>
        void admissibleNodes(const Model& model,
                             std::vector<Correspondence>& nodes,
                             std::vector<int32_t>& ids)
        {
            const int32_t nL = model.leftCount();
            const int32_t nR = model.rightCount();
            nodes.clear();
            ids.assign(size_t(nL) * size_t(nR), -1);
            for (int32_t l = 0; l < nL; ++l)
                for (int32_t r = 0; r < nR; ++r)
                    if (model.admissible(l, r))
                    {
                        ids[size_t(l) * nR + r] =
                            static_cast<int32_t>(nodes.size());
                        nodes.push_back({l, r});
                    }
        }
    }  // namespace detail

    template <class Model>
    bool buildNodesFirst(const Model& model, const BuildParams& params,
                         WorkerChannel& channel, GraphRows& out)
    {
        static_assert(detail::HasBasics<Model>::value,
                      "model lacks leftCount/rightCount/admissible");
        static_assert(detail::HasNodeGen<Model>::value,
                      "model lacks a nodes-first consistentWith");
        const int32_t nL = model.leftCount();
        const int32_t nR = model.rightCount();
        std::vector<Correspondence> nodes;
        std::vector<int32_t> ids;
        detail::admissibleNodes(model, nodes, ids);

        const size_t total = nodes.size();
        std::vector<std::vector<int32_t>> rows(total);
        std::atomic<size_t> next{0};
        std::atomic<bool> stop{false};
        constexpr size_t kBlock = 64;
        int lastPct = -1;

        detail::runWorkers(
            params.numWorkers,
            [&](size_t tid)
            {
                std::vector<Correspondence> partners;
                while (!stop.load(std::memory_order_relaxed))
                {
                    if (channel.should_cancel())
                    {
                        stop.store(true);
                        return;
                    }
                    size_t b = next.fetch_add(kBlock);
                    if (b >= total) return;
                    size_t e = std::min(b + kBlock, total);
                    for (size_t i = b; i < e; ++i)
                    {
                        const Correspondence a = nodes[i];
                        partners.clear();
                        model.consistentWith(a, partners);
                        auto& row = rows[i];
                        for (const auto& c : partners)
                        {
                            if (c.left < 0 || c.left >= nL ||
                                c.right < 0 || c.right >= nR ||
                                c.left == a.left || c.right == a.right)
                                continue;
                            int32_t id =
                                ids[size_t(c.left) * nR + c.right];
                            if (id >= 0) row.push_back(id);
                        }
                        std::sort(row.begin(), row.end());
                        row.erase(std::unique(row.begin(), row.end()),
                                  row.end());
                    }
                    if (tid == 0)
                        detail::reportProgress(channel, e, total,
                                               lastPct);
                }
            });
        if (stop.load()) return false;
        detail::finalizeRows(std::move(nodes), std::move(rows),
                             params.numWorkers, out);
        return true;
    }

    template <class Model>
    bool buildEdgesFirst(const Model& model, const BuildParams& params,
                         WorkerChannel& channel, GraphRows& out)
    {
        static_assert(detail::HasBasics<Model>::value,
                      "model lacks leftCount/rightCount/admissible");
        static_assert(detail::HasTupleGen<Model>::value,
                      "model lacks an edges-first consistentWith");
        constexpr int K = Model::arity;
        static_assert(K == 2 || K == 3, "arity must be 2 or 3");
        const int32_t nL = model.leftCount();
        const int32_t nR = model.rightCount();
        std::vector<Correspondence> nodes;
        std::vector<int32_t> ids;
        detail::admissibleNodes(model, nodes, ids);

        size_t numWorkers = std::max<size_t>(1, params.numWorkers);
        std::vector<detail::EdgeAccumulator> accs(numWorkers);
        // a unit is one leading pair (i, j), i < j, of the left tuple
        const size_t total = size_t(nL) * size_t(nL);
        std::atomic<size_t> next{0};
        std::atomic<bool> stop{false};
        constexpr size_t kBlock = K == 2 ? 1024 : 16;
        int lastPct = -1;

        detail::runWorkers(
            numWorkers,
            [&](size_t tid)
            {
                auto& acc = accs[tid];
                std::vector<TupleMatch<K>> matches;
                std::array<int32_t, K> tuple;
                auto emit = [&]()
                {
                    matches.clear();
                    model.consistentWith(tuple, matches);
                    for (const auto& m : matches)
                    {
                        int32_t id[K];
                        bool ok = true;
                        for (int p = 0; p < K && ok; ++p)
                        {
                            int32_t l = m.left[p], r = m.right[p];
                            if (l < 0 || l >= nL || r < 0 || r >= nR)
                            {
                                ok = false;
                                break;
                            }
                            for (int q = 0; q < p; ++q)
                                if (m.left[q] == l || m.right[q] == r)
                                    ok = false;
                            id[p] = ids[size_t(l) * nR + r];
                            if (id[p] < 0) ok = false;
                        }
                        if (!ok) continue;
                        for (int p = 0; p < K; ++p)
                            for (int q = p + 1; q < K; ++q)
                                acc.add(id[p], id[q]);
                    }
                };
                while (!stop.load(std::memory_order_relaxed))
                {
                    if (channel.should_cancel())
                    {
                        stop.store(true);
                        return;
                    }
                    size_t b = next.fetch_add(kBlock);
                    if (b >= total) return;
                    size_t e = std::min(b + kBlock, total);
                    for (size_t u = b; u < e; ++u)
                    {
                        int32_t i = int32_t(u / nL);
                        int32_t j = int32_t(u % nL);
                        if (j <= i) continue;
                        tuple[0] = i;
                        tuple[1] = j;
                        if constexpr (K == 2) { emit(); }
                        else
                        {
                            // A block of triples can take seconds on
                            // dense inputs; stay cancellable inside it.
                            for (int32_t k = j + 1; k < nL; ++k)
                            {
                                if (stop.load(
                                        std::memory_order_relaxed))
                                    return;
                                if (channel.should_cancel())
                                {
                                    stop.store(true);
                                    return;
                                }
                                tuple[2] = k;
                                emit();
                            }
                        }
                    }
                    if (tid == 0)
                        detail::reportProgress(channel, e, total,
                                               lastPct);
                }
            });
        if (stop.load()) return false;
        detail::finalizeEdges(std::move(nodes), accs, params.minSupport,
                              out);
        return true;
    }

    // Auto prefers nodes-first when the model provides it.
    template <class Model>
    bool buildGraph(const Model& model, const BuildParams& params,
                    WorkerChannel& channel, GraphRows& out)
    {
        constexpr bool hasNodes = detail::HasNodeGen<Model>::value;
        constexpr bool hasTuples = detail::HasTupleGen<Model>::value;
        static_assert(hasNodes || hasTuples,
                      "model provides no consistentWith generator");
        BuildOrder order = params.order;
        if (order == BuildOrder::Auto)
            order = hasNodes ? BuildOrder::NodesFirst
                             : BuildOrder::EdgesFirst;
        if (order == BuildOrder::NodesFirst)
        {
            if constexpr (hasNodes)
                return buildNodesFirst(model, params, channel, out);
        }
        else
        {
            if constexpr (hasTuples)
                return buildEdgesFirst(model, params, channel, out);
        }
        channel.error("model does not support the requested order");
        return false;
    }

}  // namespace AlignCalc

#endif
