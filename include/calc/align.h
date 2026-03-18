#ifndef SHOECOMP_CALC_ALIGN
#define SHOECOMP_CALC_ALIGN

#include "calc/workerChannel.h"

namespace shoecomp
{
    struct AlignResult;
    struct ImageData;

    // Run automatic alignment on a background thread.
    // Compares |left| and |right| images, communicates
    // progress via |channel|, and writes the estimated
    // transform into |result|.
    void runAutoAlign(const ImageData& left,
                      const ImageData& right,
                      WorkerChannel& channel,
                      AlignResult& result);
}  // namespace shoecomp

#endif
