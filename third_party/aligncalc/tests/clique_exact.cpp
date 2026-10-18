// searchCliques against an exact Bron-Kerbosch enumeration on random
// graphs (sparse with a planted clique, dense, mid-density planted):
// the set of maximum cliques matches, 1 and 8 workers agree,
// numResults keeps the first results, lowerBound above the clique
// number finds nothing, upperBound caps the clique size.
#include "aligncalc/correspondence.h"
#include "aligncalc/graphBuild.h"
#include "check.h"
#include <algorithm>
#include <random>
#include <set>
using namespace AlignCalc;

namespace
{
    using Clique = std::vector<int>;

    bool adjacent(const GraphRows& g, int a, int b)
    {
        const int32_t* r = g.row(a);
        return std::binary_search(r, r + g.degree(a), b);
    }

    // all maximum cliques (Bron-Kerbosch with pivot)
    void bronKerbosch(const GraphRows& g, Clique& R, Clique P, Clique X,
                      size_t& best, std::set<Clique>& all)
    {
        if (P.empty() && X.empty())
        {
            if (R.size() > best)
            {
                best = R.size();
                all.clear();
            }
            if (R.size() == best)
            {
                Clique c = R;
                std::sort(c.begin(), c.end());
                all.insert(c);
            }
            return;
        }
        if (R.size() + P.size() < best) return;
        int pivot = !P.empty() ? P[0] : X[0];
        size_t pivotDeg = 0;
        for (int u : P)
        {
            size_t d = 0;
            for (int v : P) d += adjacent(g, u, v);
            if (d > pivotDeg) pivotDeg = d, pivot = u;
        }
        Clique cand;
        for (int v : P)
            if (!adjacent(g, pivot, v)) cand.push_back(v);
        for (int v : cand)
        {
            Clique P2, X2;
            for (int u : P)
                if (adjacent(g, v, u)) P2.push_back(u);
            for (int u : X)
                if (adjacent(g, v, u)) X2.push_back(u);
            R.push_back(v);
            bronKerbosch(g, R, P2, X2, best, all);
            R.pop_back();
            P.erase(std::find(P.begin(), P.end(), v));
            X.push_back(v);
        }
    }

    // node i = correspondence (i, 0), so a clique is identified by
    // its left ids
    GraphRows randomGraph(std::mt19937& rng, int n, double p,
                          int planted)
    {
        std::vector<std::vector<int32_t>> rows(n);
        std::vector<Correspondence> nodes(n);
        for (int i = 0; i < n; ++i) nodes[i] = {i, 0};
        std::uniform_real_distribution<double> U(0, 1);
        for (int i = 0; i < n; ++i)
            for (int j = i + 1; j < n; ++j)
                if (U(rng) < p)
                {
                    rows[i].push_back(j);
                    rows[j].push_back(i);
                }
        std::vector<int> pl;
        for (int i = 0; i < planted; ++i) pl.push_back(rng() % n);
        for (int a : pl)
            for (int b : pl)
                if (a != b) rows[a].push_back(b);
        for (auto& r : rows)
        {
            std::sort(r.begin(), r.end());
            r.erase(std::unique(r.begin(), r.end()), r.end());
        }
        GraphRows g;
        detail::finalizeRows(std::move(nodes), std::move(rows), 2, g);
        return g;
    }

    std::set<Clique> leftIds(
        const std::vector<std::vector<Correspondence>>& cliques)
    {
        std::set<Clique> out;
        for (const auto& c : cliques)
        {
            Clique v;
            for (const Correspondence& x : c) v.push_back(x.left);
            out.insert(v);
        }
        return out;
    }
}  // namespace

int main()
{
    std::mt19937 rng(42);
    Progress progress;
    int trials = 0;
    for (int t = 0; t < 600; ++t)
    {
        int n, planted;
        double p;
        switch (t % 3)
        {
            case 0:
                n = 30 + rng() % 170;
                p = 0.02 + (rng() % 20) / 100.0;
                planted = 4 + rng() % 20;
                break;
            case 1:
                n = 20 + rng() % 40;
                p = 0.5 + (rng() % 40) / 100.0;
                planted = 0;
                break;
            default:
                n = 40 + rng() % 80;
                p = 0.3 + (rng() % 30) / 100.0;
                planted = 8 + rng() % 15;
                break;
        }
        const GraphRows g = randomGraph(rng, n, p, planted);
        if (g.size() == 0) continue;
        Clique R, P, X;
        for (size_t i = 0; i < g.size(); ++i) P.push_back(int(i));
        size_t best = 0;
        std::set<Clique> exact;
        bronKerbosch(g, R, P, X, best, exact);
        if (best < 3 || exact.size() > 4000) continue;
        ++trials;
        std::set<Clique> exactIds;
        for (const Clique& c : exact)
        {
            Clique v;
            for (int x : c) v.push_back(g.nodes[x].left);
            exactIds.insert(v);
        }

        CliqueParams cp;
        cp.numResults = 1u << 30;
        cp.lowerBound = 3;
        std::vector<std::vector<Correspondence>> r1, r8;
        cp.numWorkers = 1;
        const bool ok1 = searchCliques(g, cp, progress, r1);
        cp.numWorkers = 8;
        const bool ok8 = searchCliques(g, cp, progress, r8);
        if (!CHECKF(ok1 && ok8, "trial %d", t)) continue;
        CHECKF(leftIds(r1) == exactIds,
               "trial %d: maximum cliques differ from the exact set",
               t);
        CHECKF(r1 == r8, "trial %d: 1 vs 8 workers differ", t);

        // numResults: the first k of the full list
        cp.numResults = 2;
        std::vector<std::vector<Correspondence>> rk;
        searchCliques(g, cp, progress, rk);
        CHECKF(rk.size() == std::min<size_t>(2, r1.size()) &&
                   std::equal(rk.begin(), rk.end(), r1.begin()),
               "trial %d: numResults truncation", t);

        // lowerBound above the clique number: nothing
        cp.lowerBound = int32_t(best) + 1;
        std::vector<std::vector<Correspondence>> rl;
        CHECKF(!searchCliques(g, cp, progress, rl) && rl.empty(),
               "trial %d: lowerBound > clique number found cliques", t);

        // upperBound: cliques of exactly the cap
        cp.lowerBound = 3;
        cp.upperBound = 3;
        cp.numResults = 5;
        std::vector<std::vector<Correspondence>> ru;
        bool capOk = searchCliques(g, cp, progress, ru) && !ru.empty();
        std::vector<int> idOf(n, -1);
        for (size_t i = 0; i < g.size(); ++i)
            idOf[g.nodes[i].left] = int(i);
        for (const auto& c : ru)
        {
            if (c.size() != 3) capOk = false;
            for (size_t a = 0; a < c.size(); ++a)
                for (size_t b = a + 1; b < c.size(); ++b)
                    if (!adjacent(g, idOf[c[a].left], idOf[c[b].left]))
                        capOk = false;
        }
        CHECKF(capOk, "trial %d: upperBound cap", t);
    }
    printf("%d graphs compared\n", trials);
    CHECK(trials >= 500);
    return checkResult();
}
