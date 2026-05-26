#include "aligncalc.internal.h"
#include "clq/clqmtch.h"
#include <iostream>

namespace AlignCalc
{
    namespace RT
    {
        static constexpr int kchoose2(int32_t k)
        {
            return (k * (k - 1)) / 2;
        }

        bool fillGraph(const DoubleMatrixR& left,
                       const DoubleMatrixR& right,
                       const RTSParams& params, WorkerChannel& channel,
                       GraphInfo& graph)
        {
            SPMCQueue<TaskInfo, 128> work_q;
            MPSCQueue<TaskInfo, 128> result_q;
            bool others_inited = false;
            std::vector<std::thread> workers;
            Updater updater;
            //
            int32_t i1 = 0;
            int32_t i2 = 0;
            int32_t left_count = static_cast<int32_t>(left.rows());
            int32_t right_count = static_cast<int32_t>(right.rows());
            int32_t N1 = kchoose2(left_count);
            int32_t N2 = kchoose2(right_count);
            //
            TaskInfo query;
            TaskInfo answer;
            bool queue_full = false;

            channel.report(0.25f, "finding matchable points...");
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
                                while (work_q.pop(w))
                                {
                                    if (processTaskInfo(left, right, w,
                                                        params.epsilon))
                                    {
                                        result_q.push(w);
                                    }
                                }
                            });
                    }
                }

                // consume results, and update graph
                while (result_q.pop(answer))
                {
                    updater.updateAll(graph, answer);
                }
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
            while (result_q.pop(answer))
            {
                updater.updateAll(graph, answer);
            }
            return true;
        }
    } /* namespace RT */

} /* namespace AlignCalc */
