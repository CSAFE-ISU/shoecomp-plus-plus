#ifndef ALIGNCALC_GRAPHINFO_INTERNAL_H
#define ALIGNCALC_GRAPHINFO_INTERNAL_H
#include <cstdint>
#include <vector>
#include <cstring>
#include <unordered_map>
#include <unordered_set>

namespace AlignCalc
{
    struct Node
    {
        int32_t ind1, ind2;
        bool operator==(const Node &other) const
        {
            return (ind1 == other.ind1) && (ind2 == other.ind2);
        }
        bool operator<(const Node &other) const
        {
            return (ind1 == other.ind1) ? (ind2 < other.ind2)
                                        : (ind1 < other.ind1);
        }
        bool operator>(const Node &other) const
        {
            return (ind1 == other.ind1) ? (ind2 > other.ind2)
                                        : (ind1 > other.ind1);
        }

        size_t hash() const
        {
            uint64_t bits;
            std::memcpy(&bits, this, sizeof(Node));
            return std::hash<uint64_t>{}(bits);
        }
    };
    static_assert(sizeof(Node) == 8);

    struct NodeHash
    {
        size_t operator()(const Node &node) const
        {
            return node.hash();
        }
    };

    struct Edge
    {
        Node v1, v2;
        bool operator==(const Edge &other) const
        {
            return (v1 == other.v1) && (v2 == other.v2);
        }

        size_t hash() const
        {
            size_t h1 = v1.hash();
            size_t h2 = v2.hash();
            // https://www.boost.org/doc/libs/1_35_0/doc/html/boost/hash_combine_id241013.html
            h1 ^= h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2);
            return h1;
        }
    };

    struct EdgeHash
    {
        size_t operator()(const Edge &edge) const
        {
            return edge.hash();
        }
    };

    class GraphInfo
    {
        int32_t N;

       public:
        std::unordered_map<Node, int32_t, NodeHash> nodemap;
        std::unordered_set<Edge, EdgeHash> edges;
        void addNode(Node &);
        void addEdge(Node &, Node &);
        void reset();
    };

} /* namespace AlignCalc */

#endif
