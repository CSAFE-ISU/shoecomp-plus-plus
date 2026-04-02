#include "aligncalc.internal.h"
#include "clqmtch.h"
#include <iostream>

namespace AlignCalc
{
    static constexpr int kchoose3(int32_t k)
    {
        return (k * (k - 1) * (k - 2)) / 6;
    }

    bool RTSFillGraph(const DoubleMatrixR& left,
                      const DoubleMatrixR& right,
                      const RTSParams& params, WorkerChannel& channel,
                      GraphInfo& graph)
    {
        SPMCQueue<TaskInfo, 128> work_q;
        MPSCQueue<TaskInfo, 128> result_q;
        bool others_inited = false;
        std::vector<std::thread> workers;
        //
        int32_t i1 = 0;
        int32_t i2 = 0;
        int32_t left_count = static_cast<int32_t>(left.rows());
        int32_t right_count = static_cast<int32_t>(right.rows());
        int32_t N1 = kchoose3(left_count);
        int32_t N2 = kchoose3(right_count);
        //
        TaskInfo query;
        TaskInfo answer;
        bool queue_full = false;

        while (i1 < N1)
        {
            if (channel.should_cancel())
            {
                i1 = 0;
                // if other threads are inited,
                // they will stop result_q
                // otherwise it doesn't matter
                break;
            }

            queue_full = false;

            // produce task
            while (i1 < N1 && !queue_full)
            {
                while (i2 < N2)
                {
                    loadTaskInfo(i1, left_count, i2, right_count,
                                 query);
                    if (!work_q.try_push(query))
                    {
                        queue_full = true;
                        break;
                    }
                    else { ++i2; }
                }
                if (queue_full) { break; }
                else if (i2 >= N2)
                {
                    i2 = 0;
                    ++i1;
                }
            }

            if (!others_inited)
            {
                others_inited = true;
                // initialize other threads
                workers.reserve(params.numWorkers);
                for (size_t tid = 0; tid < params.numWorkers; ++tid)
                {
                    workers.emplace_back(
                        [&work_q, &result_q, &left, &right, &params]
                        {
                            TaskInfo w;
                            double delta = params.delta;
                            double epsilon = params.epsilon;
                            while (work_q.pop(w))
                            {
                                if (processTaskInfo(left, right, w,
                                                    delta, epsilon))
                                {
                                    result_q.push(w);
                                }
                            }
                        });
                }
            }

            // consume results, and update graph
            while (result_q.pop(answer)) { graph.updateAll(answer); }
            channel.report((i1 * 1.0f) / N1, "Processing...");
        }
        work_q.stop();

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

        // drain result_q in case something is left
        while (result_q.pop(answer)) { graph.updateAll(answer); }
        return true;
    }

    bool RTSAlignment(const DoubleMatrixR& left_pts,
                      const DoubleMatrixR& right_pts,
                      const RTSParams& params, WorkerChannel& channel,
                      std::vector<MatchedPoints>& results)
    {
        GraphInfo g_info;
        clqmtch::StackDFS S(params.numWorkers, params.numResults,
                            params.lowerBound, params.upperBound);
        clqmtch::Graph G;
        std::vector<std::unordered_set<int32_t>> cliques;
        g_info.reset();
        //
        if (!RTSFillGraph(left_pts, right_pts, params, channel, g_info))
        {
            channel.error("could not fill graph");
            return false;
        }

        if (!G.init(channel, g_info) ||
            !S.processGraph(channel, G, cliques))
        {
            channel.error("clique search failed");
            return false;
        }
        if (cliques.empty())
        {
            channel.error("no alignments found");
            return false;
        }

        for (const auto& clq : cliques)
        {
            if (clq.size() < params.lowerBound) continue;
            results.push_back(MatchedPoints{});
            MatchedPoints& match = results.back();
            match.N = clq.size();
            match.left_pts.resize(match.N, 3);
            match.right_pts.resize(match.N, 3);
            //
            int i = 0;
            for (const i32& c : clq)
            {
                const clqmtch::Vertex& v = G.vertices[c];
                match.left_pts.row(i) = left_pts.row(v.ind1);
                match.right_pts.row(i) = right_pts.row(v.ind2);
                ++i;
            }
        }
        return true;
    }

} /* namespace AlignCalc */
