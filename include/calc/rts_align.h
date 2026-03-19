#ifndef SHOECOMP_CALC_RTS_ALIGN
#define SHOECOMP_CALC_RTS_ALIGN

#include "eigen/Eigen/Dense"
#include "calc/align.h"
#include "calc/workerChannel.h"
#include "calc/multiQueue.h"
#include <cstdint>
#include <unordered_set>

namespace shoecomp
{

    static constexpr size_t MAX_POINTS = 256;

    using DoubleMatrixR =
        Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic,
                      Eigen::RowMajor>;

    struct TaskInfo
    {
        int32_t i1, j1, k1;
        int32_t i2, j2, k2;
        uint8_t check[8];
    };

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

    struct Triangle2D
    {
        double as, bs, cs; /* sides */
        double at, bt, ct; /* angles */
        /* as is the side opposite to at */
        void construct(const DoubleMatrixR &, const int32_t,
                       const int32_t, const int32_t);
        bool valid() const;
        bool compare(const Triangle2D &, TaskInfo &, double,
                     double) const;
    };

    class GraphInfo
    {
        Node v1, v2, v3;
        int32_t N;

        void insert(Node &);
        void update0(const TaskInfo &);
        void update1(const TaskInfo &);
        void update2(const TaskInfo &);
        void update3(const TaskInfo &);
        void update4(const TaskInfo &);
        void update5(const TaskInfo &);

       public:
        std::unordered_map<Node, int32_t, NodeHash> nodemap;
        std::unordered_set<Edge, EdgeHash> edges;

        void updateAll(const TaskInfo &);
    };

    void loadTaskInfo(int32_t, int32_t, int32_t, int32_t, TaskInfo &);
    bool processTaskInfo(const DoubleMatrixR &, const DoubleMatrixR &,
                         TaskInfo &);
    bool RTSFillGraph(const DoubleMatrixR &, const DoubleMatrixR &,
                      WorkerChannel &, GraphInfo &, size_t);
}  // namespace shoecomp

#endif
