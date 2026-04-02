#ifndef SHOECOMP_CALC_ALIGN
#define SHOECOMP_CALC_ALIGN

#include "aligncalc/workerChannel.h"
#include "aligncalc/aligncalc.h"

namespace shoecomp
{
    struct AlignResult;
    struct ImageData;

    // Run automatic alignment on a background thread.
    // Compares |left| and |right| images, communicates
    // progress via |channel|, and writes the estimated
    // transform into |result|.
    void runAutoAlign(ImageData& left, ImageData& right,
                      WorkerChannel& channel, AlignResult& result,
                      const AlignCalc::RTSParams& params);
    void runRTSAlign(ImageData& left, ImageData& right,
                     WorkerChannel& channel, AlignResult& result,
                     const AlignCalc::RTSParams& params);
}  // namespace shoecomp

#endif
