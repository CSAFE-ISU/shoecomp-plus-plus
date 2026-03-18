#include "calc/align.h"
#include "ui/alignDialog.h"
#include "ui/imageCanvas.h"
#include <chrono>
#include <thread>

namespace shoecomp
{
    void runAutoAlign(const ImageData& left,
                      const ImageData& right,
                      WorkerChannel& channel,
                      AlignResult& result)
    {
        (void)left;
        (void)right;
        // Dummy implementation: 10 steps, 500 ms each.
        // A real implementation would compute feature
        // correspondences and estimate the rigid
        // transform between the two images.
        for (int i = 0; i < 10; ++i)
        {
            if (channel.should_cancel())
            {
                channel.cancelled();
                return;
            }
            std::this_thread::sleep_for(
                std::chrono::milliseconds(500));
            channel.report((i + 1) * 0.1f,
                           "Processing...");
        }

        result.rotation = 0.0f;
        result.translationX = 0.0f;
        result.translationY = 0.0f;
        result.scale = 1.0f;
        channel.done();
    }
}  // namespace shoecomp
