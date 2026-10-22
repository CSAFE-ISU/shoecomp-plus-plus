#ifndef SHOECOMP_CALC_ALIGN3D
#define SHOECOMP_CALC_ALIGN3D

#include <string>
#include <utility>
#include <vector>

#include "aligncalc/alignTypes.h"
#include "aligncalc/distinct.h"
#include "aligncalc/progress.h"
#include <Eigen/Geometry>
#include "calc/workerChannel.h"
#include "calc/cylinderFit.h"
#include "formats/x3p.h"

namespace shoecomp
{
    struct StriationAlignParams
    {
        double angleTolDeg = 5.0;
        double sepTolUm = 5.0;
        double maxLengthRatio = 0.0;  // > 1: bound on longer/shorter
        double extentScale = 0.0;     // see AlignCalc::LineParams
        double anchorTolUm = 1.0;     // LineParams::anchorTol, um
        size_t numWorkers = 4;
        size_t numResults = 2;  // distinct alignments, best first
        int minLines = 10;
        int maxLines = 0;  // 0 = fewer of the two line counts
        // Alignments after the best need at least this fraction of
        // its matched lines (and minLines).
        double distinctMinFrac = 0.7;
        AlignCalc::DistinctMode distinct =
            AlignCalc::DistinctMode::Explained;
        // Reject alignments rotating one land by more than this
        // (degrees; e.g. 180 = reversed end-for-end); 0 = off.
        double maxRotationDeg = 0.0;
    };

    struct StriationAlignment
    {
        Eigen::Isometry3d leftToRight = Eigen::Isometry3d::Identity();
        std::vector<std::pair<int, int>> pairs;  // row indices
        Eigen::Vector3d axis = Eigen::Vector3d::UnitZ();
        double rmsPerp = 0.0;  // metres
        bool axialWeak = false;
        int explained = 0;  // pairs it accounts for (searchDistinct)
    };

    // The striation lines in |surf|'s peaks[]. Malformed entries are
    // skipped; |peakIndex| (optional) gets each line's peaks[] index.
    std::vector<StriationLine> surfaceStriations(
        const Surface3D& surf, std::vector<int>* peakIndex = nullptr);

    // One row per line: midpoint, direction, length, ridge flag.
    AlignCalc::DoubleMatrixR striationRows(
        const std::vector<StriationLine>& lines);

    // Rigid registration of two lands from their striation lines:
    // correspondence graph (RigidLines3D, ridges only match ridges),
    // maximum cliques, and a line fit per clique, best first.
    // NoMatch when no consistent set of striations exists; NoMatch
    // and Error also post |message| (optional) to |channel| as an
    // error.
    AlignCalc::Status runStriationAlign(
        const AlignCalc::DoubleMatrixR& left,
        const AlignCalc::DoubleMatrixR& right,
        const StriationAlignParams& params, WorkerChannel& channel,
        std::vector<StriationAlignment>& out,
        std::string* message = nullptr);
}  // namespace shoecomp

#endif
