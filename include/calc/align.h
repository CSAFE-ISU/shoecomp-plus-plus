#ifndef SHOECOMP_CALC_ALIGN
#define SHOECOMP_CALC_ALIGN

#include <vector>
#include "aligncalc/workerChannel.h"
#include "aligncalc/aligncalc.h"
#include "ui/imageCanvas2d.h"

namespace shoecomp
{
    struct AlignState;

    // Run automatic alignment on a background thread.
    // Compares |left| and |right| images, communicates
    // progress via |channel|, and writes the estimated
    // transforms into |results|.
    void runAutoAlign(ImageCanvas2D::ImageData& left,
                      ImageCanvas2D::ImageData& right,
                      WorkerChannel& channel,
                      std::vector<AlignState>& results,
                      const AlignCalc::RTSParams& params);
    void runRTSAlign(ImageCanvas2D::ImageData& left,
                     ImageCanvas2D::ImageData& right,
                     WorkerChannel& channel,
                     std::vector<AlignState>& results,
                     const AlignCalc::RTSParams& params);
}  // namespace shoecomp

#endif
