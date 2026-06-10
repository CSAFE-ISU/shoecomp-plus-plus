#include "calc/align.h"
#include "ui/alignDialog.h"
#include "ui/imageCanvas.h"
#include <chrono>
#include <thread>

namespace shoecomp
{

    static void dummyAlign(ImageCanvas::ImageData& left,
                           ImageCanvas::ImageData& right,
                           WorkerChannel& channel,
                           std::vector<AlignState>& results)
    {
        (void)left;
        (void)right;
        // Dummy implementation: 10 steps, 500 ms each.
        for (int i = 0; i < 10; ++i)
        {
            if (channel.should_cancel())
            {
                channel.cancelled();
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            channel.report((i + 1) * 0.1f, "Processing...");
        }

        results.push_back(AlignState{});
        channel.done();
    }

    void runAutoAlign(ImageCanvas::ImageData& left,
                      ImageCanvas::ImageData& right,
                      WorkerChannel& channel,
                      std::vector<AlignState>& results,
                      const AlignCalc::RTSParams& params)
    {
        results.clear();
        // dummyAlign(left, right, channel, results);
        runRTSAlign(left, right, channel, results, params);
    }
}  // namespace shoecomp
