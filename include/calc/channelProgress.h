#ifndef SHOECOMP_CALC_CHANNELPROGRESS_H
#define SHOECOMP_CALC_CHANNELPROGRESS_H

#include "aligncalc/progress.h"
#include "calc/workerChannel.h"

namespace shoecomp
{
    // aligncalc progress and cancellation through a WorkerChannel; the
    // library reports in 5% steps, so a run stays well under the ring's
    // 64 messages.
    class ChannelProgress : public AlignCalc::Progress
    {
        WorkerChannel& channel_;

       public:
        explicit ChannelProgress(WorkerChannel& channel)
            : channel_(channel)
        {
        }
        void report(float fraction, const char* stage) override
        {
            channel_.report(fraction, stage);
        }
        bool cancelled() const override
        {
            return channel_.should_cancel();
        }
    };
}  // namespace shoecomp

#endif
