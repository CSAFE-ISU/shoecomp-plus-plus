#ifndef ALIGNCALC_DISTINCT_H
#define ALIGNCALC_DISTINCT_H
#include "correspondence.h"
#include "workerChannel.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace AlignCalc
{
    // What a round of searchDistinct sets aside before searching
    // again.
    enum class DistinctMode
    {
        // the clique's pairs and every pair sharing a left or right
        // point with it
        Disjoint,
        // the clique's pairs and every pair its transform explains
        Explained,
    };

    struct DistinctParams
    {
        size_t numWorkers = 2;
        size_t numResults = 2;
        int32_t lowerBound = 3;
        int32_t upperBound = 500;
        const CliqueConstraint* constraint = nullptr;
        DistinctMode mode = DistinctMode::Explained;
        // Alignments after the first need this fraction of its size
        // (and lowerBound).
        double minFrac = 0.7;
    };

    enum class DistinctStatus
    {
        Ok,
        NoMatch,      // no clique reaches lowerBound
        FitFailed,    // first round: no clique could be fitted
        AllRejected,  // every fitted clique failed accept()
        Cancelled,
    };

    // |g| restricted to the nodes with keep[i] set (ids renumbered in
    // order; nodes left without neighbours are dropped).
    GraphRows keepNodes(const GraphRows& g,
                        const std::vector<char>& keep);

    // Distinct alignments, best first. Each round takes a maximum
    // clique of what is left (deterministic), fits it, sets aside what
    // |params.mode| says, and searches the rest again, so later
    // results are different placements rather than variants of one.
    // |fitter| supplies
    //   bool fit(const std::vector<Correspondence>&);  // false: next
    //   tie bool explains(Correspondence) const;  // under the last fit
    //   bool accept() const;  // false: set aside (explained pairs
    //                         // only, in either mode), not reported
    //   void keep();          // report the last fit
    // Rounds after the first mute progress posts, so a final error
    // still fits the channel's ring.
    template <class Fitter>
    DistinctStatus searchDistinct(const GraphRows& graph,
                                  const DistinctParams& params,
                                  Fitter& fitter,
                                  WorkerChannel& channel)
    {
        struct MuteGuard
        {
            WorkerChannel& ch;
            ~MuteGuard() { ch.muteProgress = false; }
        } mute{channel};
        CliqueParams cp;
        cp.numWorkers = params.numWorkers;
        cp.numResults = 4;  // a few ties, in case a fit fails
        cp.deterministic = true;
        cp.lowerBound = params.lowerBound;
        cp.upperBound = params.upperBound;
        cp.constraint = params.constraint;

        int32_t nl = 0, nr = 0;
        for (const Correspondence& c : graph.nodes)
        {
            nl = std::max(nl, c.left + 1);
            nr = std::max(nr, c.right + 1);
        }
        std::vector<char> keep(graph.size(), 1), usedL(nl, 0),
            usedR(nr, 0);
        GraphRows rest;
        const GraphRows* g = &graph;
        const size_t want = std::max<size_t>(1, params.numResults);
        // Rejected candidates also cost a round; cap them with a
        // constant (not one scaled by |want|) so the first k results
        // are the same whatever |want| is.
        const int kMaxRejected = 16;
        int rejected = 0;
        size_t found = 0;
        for (size_t round = 0; found < want; ++round)
        {
            std::vector<std::vector<Correspondence>> cliques;
            if (round > 0) channel.muteProgress = true;
            if (g->size() == 0 ||
                !searchCliques(*g, cp, channel, cliques))
            {
                if (channel.should_cancel())
                    return DistinctStatus::Cancelled;
                if (round > 0) break;
                return DistinctStatus::NoMatch;
            }
            if (round == 0)
                channel.report(0.9f, "fitting transforms...");

            const std::vector<Correspondence>* used = nullptr;
            for (const auto& clq : cliques)
                if (fitter.fit(clq))
                {
                    used = &clq;
                    break;
                }
            if (!used)
            {
                if (round > 0) break;
                return DistinctStatus::FitFailed;
            }

            // A rejected clique (e.g. a land reversed end-for-end)
            // shares its points with the real alignment, so it never
            // takes them with it.
            const bool accepted = fitter.accept();
            const bool byPoint =
                accepted && params.mode == DistinctMode::Disjoint;
            if (byPoint)
                for (const Correspondence& c : *used)
                    usedL[c.left] = usedR[c.right] = 1;
            for (size_t i = 0; i < graph.size(); ++i)
            {
                if (!keep[i]) continue;
                const Correspondence c = graph.nodes[i];
                if (std::binary_search(used->begin(), used->end(), c) ||
                    (byPoint ? usedL[c.left] || usedR[c.right]
                             : fitter.explains(c)))
                    keep[i] = 0;
            }
            if (!accepted && ++rejected > kMaxRejected) break;
            if (accepted)
            {
                if (found == 0)
                    cp.lowerBound = std::max(
                        params.lowerBound,
                        (int32_t)std::ceil(params.minFrac *
                                               (double)used->size() -
                                           1e-9));
                fitter.keep();
                ++found;
            }
            if (found >= want || channel.should_cancel()) break;
            rest = keepNodes(graph, keep);
            g = &rest;
        }
        if (channel.should_cancel()) return DistinctStatus::Cancelled;
        return found ? DistinctStatus::Ok : DistinctStatus::AllRejected;
    }
}  // namespace AlignCalc

#endif
