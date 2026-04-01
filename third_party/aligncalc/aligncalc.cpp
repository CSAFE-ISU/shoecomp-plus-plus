#include "aligncalc.internal.h"
#include "clqmtch.h"

namespace AlignCalc
{
    static constexpr int kchoose3(int32_t k)
    {
        return (k * (k - 1) * (k - 2)) / 6;
    }

    bool RTSFillGraph(const DoubleMatrixR& left,
                      const DoubleMatrixR& right,
                      WorkerChannel& channel, GraphInfo& graph,
                      size_t totalWorkers)
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
                workers.reserve(totalWorkers);
                for (size_t tid = 0; tid < totalWorkers; ++tid)
                {
                    workers.emplace_back(
                        [&work_q, &result_q, &left, &right]
                        {
                            TaskInfo w;
                            while (work_q.pop(w))
                            {
                                if (processTaskInfo(left, right, w))
                                {
                                    result_q.push(w);
                                }
                            }
                        });
                }
            }

            // consume results, and update graph
            while (result_q.pop(answer)) { graph.updateAll(answer); }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
                      std::vector<std::unordered_set<int32_t>>& results)
    {
        GraphInfo g_info;
        clqmtch::StackDFS S(params.numWorkers, params.numResults,
                            params.lowerBound, params.upperBound);
        clqmtch::Graph G;
        g_info.reset();
        //
        if (!RTSFillGraph(left_pts, right_pts, channel, g_info,
                          params.numWorkers))
        {
            channel.error("could not fill graph");
            return false;
        }

        if (!G.init(channel, g_info) ||
            !S.processGraph(channel, G, results))
        {
            channel.error("clique search failed");
            return false;
        }
        if (results.empty())
        {
            channel.error("no cliques found");
            return false;
        }

        for (const auto& clq : results)
        {
            for (const int32_t& v : clq)
            {
                printf("(%d, %d) ", G.vertices[v].ind1,
                       G.vertices[v].ind2);
            }
            printf("\n");
        }
        return true;
    }

} /* namespace AlignCalc */
