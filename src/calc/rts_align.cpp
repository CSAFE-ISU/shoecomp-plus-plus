#include "ui/alignDialog.h"
#include "ui/imageCanvas.h"
#include "calc/rts_align.h"
#include "calc/clqmtch.h"
#include <chrono>
#include <thread>
#include <iostream>

namespace shoecomp
{
    static bool extractAnnotatedPoints(ImageData& data,
                                       DoubleMatrixR& mat)
    {
        if (!data.annotations.isObject() ||
            !data.annotations.contains("points") ||
            !data.annotations["points"].isArray())
        {
            return false;
        }
        auto& pts = data.annotations["points"].getArray();
        if (pts.size() == 0) { return false; }
        if (pts.size() > MAX_POINTS) { return false; }
        mat.resize(pts.size(), 3);
        for (size_t i = 0; i < pts.size(); ++i)
        {
            auto& el = pts[i];
            if (!el.isObject()) return false;
            if (!el.contains("x") || !el["x"].isNumber()) return false;
            if (!el.contains("y") || !el["y"].isNumber()) return false;
            if (!el.contains("type") || !el["type"].isString())
                return false;
            mat(i, 0) = el["x"].getNumber();
            mat(i, 1) = el["y"].getNumber();
            mat(i, 2) = static_cast<double>(
                stringToPointType(el["type"].getString()));
        }
        return true;
    }

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

    void runRTSAlign(ImageData& left, ImageData& right,
                     WorkerChannel& channel, AlignResult& result)
    {
        size_t numWorkers = 1;
        size_t numResults = 32;
        i32 lowerBound = 3;
        i32 upperBound = 500;
        DoubleMatrixR left_pts;
        DoubleMatrixR right_pts;
        GraphInfo g_info;
        clqmtch::StackDFS S(numWorkers, numResults, lowerBound,
                            upperBound);
        clqmtch::Graph G;
        std::vector<std::unordered_set<i32>> results;
        g_info.reset();

        channel.report(0.0f, "loading points...");
        if (!extractAnnotatedPoints(left, left_pts))
        {
            channel.error(
                "left image does not have 1 to 256 valid points");
            channel.cancelled();
            return;
        }
        if (!extractAnnotatedPoints(right, right_pts))
        {
            channel.error(
                "right image does not have 1 to 256 valid points");
            channel.cancelled();
            return;
        }

        if (!RTSFillGraph(left_pts, right_pts, channel, g_info,
                          numWorkers))
        {
            channel.error("could not fill graph");
            channel.cancelled();
            return;
        }

        if (!G.init(channel, g_info) || !S.processGraph(channel, G, results))
        {
            channel.error("clique search failed");
            channel.cancelled();
            return;
        }
        if (results.empty())
        {
            channel.error("no cliques found");
            channel.cancelled();
            return;
        }

        for (const auto& clq : results)
        {
            for (const int32_t& v : clq) { printf("(%d, %d) ", G.vertices[v].ind1, G.vertices[v].ind2); }
            printf("\n");
        }

        //
        channel.done();
    }
}  // namespace shoecomp
