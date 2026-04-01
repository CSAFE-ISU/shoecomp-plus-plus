#ifndef ALIGNCALC_CLQMTCH_H
#define ALIGNCALC_CLQMTCH_H
#include "aligncalc.internal.h"
#include <vector>
#include <memory>
#include <cstdint>
#include <atomic>

#ifdef _MSC_VER
#include <intrin.h>
#pragma intrinsic(_BitScanReverse64)
#pragma intrinsic(_lzcnt_u64)
#endif

namespace AlignCalc
{

    typedef uint64_t u64;
    typedef uint32_t u32;
    typedef uint8_t u8;
    typedef int32_t i32;

    namespace clqmtch
    {
        constexpr u64 ALL_ONES = 0xFFFFFFFFFFFFFFFFULL;
        constexpr u64 MSB_64 = 0x8000000000000000ULL;

        inline u64 clz(const u64 n)
        {
#ifdef _MSC_VER
            /* MSVC docs for amd64 guarantee _lzcnt_u64 */
            return _lzcnt_u64(n);
#else
            /* minimum clang/gcc version? */
            return __builtin_clzll(n);
#endif
        }

        struct GraphBits
        {
            u64 padCover;
            u64 maxBits;
            u64* data;

            GraphBits& operator=(const GraphBits&) = delete;
            GraphBits() = default;
            GraphBits(GraphBits&& other)
            {
                this->data = other.data;
                this->maxBits = other.maxBits;
                this->padCover = other.padCover;
            }
            GraphBits(u64* ext_data, const u64 n_bits,
                      const bool cleanout = false)
            {
                this->refer_from(ext_data, n_bits, cleanout);
            }
            void copy_from(const GraphBits& other, u64* data_ptr)
            {
                this->maxBits = other.maxBits;
                this->padCover = other.padCover;
                this->data = data_ptr;
                this->copy_data(other);
            }
            void copy_data(const GraphBits& other)
            {
                u64 dlen = ((this->maxBits & 0x3fu) != 0) +
                           (this->maxBits >> 6);
                std::copy(other.data, other.data + dlen, this->data);
            }
            void refer_from(const GraphBits& other)
            {
                this->data = other.data;
                this->padCover = other.padCover;
                this->maxBits = other.maxBits;
            }
            void refer_from(u64* ext_data, const u64 n_bits,
                            const bool cleanout = false)
            {
                this->data = ext_data;  // CALLER gives me the data,
                // they should have initialized it and checked bounds
                this->maxBits = n_bits;
                this->padCover = ALL_ONES << (64 - (n_bits & 0x3fu));
                if (cleanout) this->clear();
            }
            void clear(const u64 N = 0)
            {
                u64 i = 0;
                u64 clear_len = 1 + (N >> 6);
                u64 dlen = ((this->maxBits & 0x3fu) != 0) +
                           (this->maxBits >> 6);
                if (N == 0 || clear_len > dlen) clear_len = dlen;
                for (i = 0; i < clear_len; i++) this->data[i] = 0;
            }

            void set(const u64 i)
            {
                // assert(i < this->maxBits);
                u64 mask = MSB_64 >> (i & 0x3fu);
                this->data[i >> 6] |= mask;
            };
            void reset(const u64 i)
            {
                // assert(i < this->maxBits);
                u64 mask = ~(MSB_64 >> (i & 0x3fu));
                this->data[i >> 6] &= mask;
            };
            void toggle(const u64 i)
            {
                // assert(i < this->maxBits);
                u64 mask = MSB_64 >> (i & 0x3fu);
                this->data[i >> 6] ^= mask;
            };
            bool block_empty(const u64 i) const
            {
                return (this->data[i >> 6] == 0);
            };
            bool operator[](const u64 i) const
            {
                // assert(i < this->maxBits);
                u64 mask = MSB_64 >> (i & 0x3fu);
                return (this->data[i >> 6] & mask) != 0;
            };
            u64 next(const u64 i)
            {
                if (i < maxBits)
                {
                    const u64 base = (i >> 6);
                    const u64 mask =
                        this->data[base] & (ALL_ONES >> (i & 0x3fu));
                    if (mask)
                        return (base << 6) + clz(mask);
                    else
                        return next((base + 1) << 6);
                }
                else
                    return this->maxBits;
            }
            u64 count() const;
        };

        struct Vertex
        {
            u64 elo;   // edge_list offset
            i32 N;     // # neighbors
            i32 spos;  // starting position for search
            i32 ind1;
            i32 ind2;

            void init(u64 elo, i32 N, i32 spos)
            {
                this->elo = elo;
                this->N = N;
                this->spos = spos;
            }
        };

        struct Graph
        {
            std::unique_ptr<i32[]> edge_list;
            u64 el_size;
            std::vector<Vertex> vertices;
            u64 num_vertices;
            i32 maxDegree;
            i32 maxDepth;
            //
            bool init(WorkerChannel&, GraphInfo&);
        };

        class SearchBlob
        {
           private:
            std::unique_ptr<u64[]> raw;
            GraphBits res_;

           public:
            u64 numBits;
            u64 stride;
            u64 maxDepth;
            u64 total;
            std::unique_ptr<i32[]> vstack;

            void init(u64, u64);
            GraphBits& res() { return res_; }
            void loadBitset(GraphBits&, i32, i32);
        };

        struct CliqueResult
        {
            i32 vertexID;
            std::unique_ptr<u64[]> neighbors;
            void construct(i32, const GraphBits&);
        };

        class StackDFS
        {
            const size_t numWorkers;
            const size_t numResults;
            const i32 lowerBound;
            const i32 upperBound;
            SPMCQueue<i32, 32> work_q;
            MPSCQueue<CliqueResult, 128> result_q;
            std::vector<SearchBlob> searchBlobs;
            std::atomic<i32> max_clique_size;

            //
           public:
            StackDFS(size_t nw, size_t nr, i32 low, i32 high)
                : numWorkers(nw), numResults(nr), lowerBound(low),
                  upperBound(high)
            {
                this->searchBlobs.resize(this->numWorkers);
            }
            bool processGraph(WorkerChannel&, const Graph&,
                              std::vector<std::unordered_set<i32>>&);
            void processVertex(WorkerChannel&, size_t, i32,
                               const Graph&);
            void update(const Graph&,
                        std::vector<std::unordered_set<i32>>&,
                        CliqueResult&&);
        };

    } /* namespace clqmtch */

} /* namespace AlignCalc */

#endif
