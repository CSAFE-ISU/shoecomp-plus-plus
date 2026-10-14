#include "calc/align3d.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "aligncalc/estimate.h"
#include "aligncalc/rigidLines3d.h"
#include "formats/x3pAnnotationIo.h"

namespace shoecomp
{
    namespace
    {
        constexpr double kPi = 3.14159265358979323846;

        enum StriationCol : int
        {
            ColMX,
            ColMY,
            ColMZ,
            ColDX,
            ColDY,
            ColDZ,
            ColLength,
            ColRidge,
            NumStriationCols
        };

        struct StriationRules : AlignCalc::LabelRules
        {
            double maxLengthRatio = 0.0;

            bool admissible(const AlignCalc::DoubleMatrixR& L,
                            int32_t l,
                            const AlignCalc::DoubleMatrixR& R,
                            int32_t r) const
            {
                if (L(l, ColRidge) != R(r, ColRidge)) return false;
                if (maxLengthRatio > 1.0)
                {
                    const double a = L(l, ColLength);
                    const double b = R(r, ColLength);
                    const double lo = std::min(a, b);
                    if (!(lo > 0.0) ||
                        std::max(a, b) > maxLengthRatio * lo)
                        return false;
                }
                return true;
            }
        };

        AlignCalc::LineCols lineCols()
        {
            AlignCalc::LineCols c;
            c.mid = {ColMX, ColMY, ColMZ};
            c.dir = {ColDX, ColDY, ColDZ};
            c.length = ColLength;
            return c;
        }
    }  // namespace

    std::vector<StriationLine> surfaceStriations(const Surface3D& surf)
    {
        std::vector<StriationLine> out;
        const jt::Json& ann = surf.annotations;
        if (!ann.isObject() || !ann.contains("peaks")) return out;
        const jt::Json& peaks = ann["peaks"];
        if (!peaks.isArray()) return out;
        for (const auto& pk : peaks.getArray())
        {
            StriationLine ln;
            if (striationLineFromJson(pk, ln)) out.push_back(ln);
        }
        return out;
    }

    AlignCalc::DoubleMatrixR striationRows(
        const std::vector<StriationLine>& lines)
    {
        AlignCalc::DoubleMatrixR m((Eigen::Index)lines.size(),
                                   NumStriationCols);
        for (size_t i = 0; i < lines.size(); ++i)
        {
            const StriationLine& s = lines[i];
            const Eigen::Vector3d mid =
                s.start + 0.5 * s.length * s.dir;
            const Eigen::Index r = (Eigen::Index)i;
            m(r, ColMX) = mid.x();
            m(r, ColMY) = mid.y();
            m(r, ColMZ) = mid.z();
            m(r, ColDX) = s.dir.x();
            m(r, ColDY) = s.dir.y();
            m(r, ColDZ) = s.dir.z();
            m(r, ColLength) = s.length;
            m(r, ColRidge) = s.isPeak ? 1.0 : 0.0;
        }
        return m;
    }

    bool runStriationAlign(const AlignCalc::DoubleMatrixR& left,
                           const AlignCalc::DoubleMatrixR& right,
                           const StriationAlignParams& params,
                           WorkerChannel& channel,
                           std::vector<StriationAlignment>& out)
    {
        out.clear();
        const int minLines = std::max(params.minLines, 2);
        if (left.rows() < minLines || right.rows() < minLines)
        {
            channel.error("both lands need detected striations");
            return false;
        }

        AlignCalc::LineParams lp;
        lp.angleTol = params.angleTolDeg * kPi / 180.0;
        lp.sepTol = params.sepTolUm * 1e-6;
        StriationRules rules;
        rules.maxLengthRatio = params.maxLengthRatio;
        const AlignCalc::LineCols cols = lineCols();
        AlignCalc::RigidLines3D<StriationRules> model(left, right, cols,
                                                      lp, rules);
        if (!model.valid())
        {
            channel.error("invalid striation lines or tolerances");
            return false;
        }

        AlignCalc::BuildParams bp;
        bp.numWorkers = params.numWorkers;
        AlignCalc::GraphRows graph;
        channel.report(0.25f, "matching striation pairs...");
        if (!AlignCalc::buildGraph(model, bp, channel, graph))
        {
            if (!channel.should_cancel())
                channel.error("could not build the match graph");
            return false;
        }

        AlignCalc::CliqueParams cp;
        cp.numWorkers = params.numWorkers;
        cp.numResults = params.numResults;
        cp.lowerBound = minLines;
        cp.upperBound = (int32_t)std::min(left.rows(), right.rows());
        std::vector<std::vector<AlignCalc::Correspondence>> cliques;
        channel.report(0.5f, "searching consistent matches...");
        if (!AlignCalc::searchCliques(graph, cp, channel, cliques))
        {
            if (!channel.should_cancel())
                channel.error("no consistent set of striations found");
            return false;
        }

        channel.report(0.9f, "fitting transforms...");
        for (const auto& clq : cliques)
        {
            AlignCalc::LineFit fit;
            if (!AlignCalc::estimateRigidLines3D(left, right, cols, clq,
                                                 fit))
                continue;
            StriationAlignment a;
            a.leftToRight = fit.transform;
            a.axis = fit.axis;
            a.rmsPerp = fit.rmsPerp;
            a.axialWeak = fit.axialWeak;
            for (const auto& c : clq)
                a.pairs.push_back({c.left, c.right});
            out.push_back(std::move(a));
        }
        if (out.empty())
        {
            channel.error("transform fit failed");
            return false;
        }
        return true;
    }
}  // namespace shoecomp
