#ifndef ALIGNCALC_CORRESPONDENCE_H
#define ALIGNCALC_CORRESPONDENCE_H
#include "export.h"
#include "progress.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace AlignCalc
{
    struct Correspondence
    {
        int32_t left;
        int32_t right;

        bool operator==(const Correspondence& o) const
        {
            return left == o.left && right == o.right;
        }
        bool operator!=(const Correspondence& o) const
        {
            return !(*this == o);
        }
        bool operator<(const Correspondence& o) const
        {
            return left != o.left ? left < o.left : right < o.right;
        }
    };

    template <int K>
    struct TupleMatch
    {
        std::array<int32_t, K> left;
        std::array<int32_t, K> right;  // right[k] matches left[k]
    };

    enum class BuildOrder
    {
        Auto,
        NodesFirst,
        EdgesFirst
    };

    struct BuildParams
    {
        BuildOrder order = BuildOrder::Auto;
        size_t numWorkers = 2;
        int32_t minSupport = 1;
    };

    // Undirected graph over correspondences in CSR form. Node ids
    // follow the lexicographic order of |nodes|; every adjacency row
    // is sorted and nodes without neighbors are dropped.
    struct GraphRows
    {
        std::vector<Correspondence> nodes;
        std::vector<uint64_t> offsets;  // nodes.size() + 1 entries
        std::vector<int32_t> adj;

        size_t size() const { return nodes.size(); }
        size_t degree(size_t i) const
        {
            return offsets[i + 1] - offsets[i];
        }
        const int32_t* row(size_t i) const
        {
            return adj.data() + offsets[i];
        }
    };

    // Optional geometric constraint for searchCliques. Every clique is
    // anchored on its two lowest vertices in the solver's (core)
    // order; the remaining members must pass the constraint against
    // that anchor, so a result is a maximum clique among those. The
    // solver filters the anchor's common neighbors before building any
    // bitset. Called from several threads at once.
    class ALIGNCALC_API CliqueConstraint
    {
       public:
        virtual ~CliqueConstraint() = default;
        // keep[k] = whether cands[k] may join a clique anchored on
        // (a, b)
        virtual void filter(Correspondence a, Correspondence b,
                            const Correspondence* cands, size_t n,
                            uint8_t* keep) const = 0;
    };

    struct CliqueParams
    {
        size_t numWorkers = 2;
        size_t numResults = 2;
        int32_t lowerBound = 3;
        int32_t upperBound = 500;
        const CliqueConstraint* constraint = nullptr;
        // Reproducible result regardless of thread timing, even when
        // more maximum cliques tie than the solver keeps (costs a
        // second search in that case).
        bool deterministic = false;
    };

    ALIGNCALC_API bool searchCliques(
        const GraphRows&, const CliqueParams&, Progress&,
        std::vector<std::vector<Correspondence>>&);

    ALIGNCALC_API bool isSymmetric(const GraphRows&);

}  // namespace AlignCalc

#endif
