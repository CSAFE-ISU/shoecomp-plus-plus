#include "calc/align3d.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "aligncalc/distinct.h"
#include <Eigen/Eigenvalues>
#include "aligncalc/estimate.h"
#include "aligncalc/rigidLines3d.h"
#include "formats/x3pAnnotationIo.h"

namespace shoecomp
{
    const char* const kNoStriationMatch =
        "no consistent set of striations found";

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

        Eigen::Vector3d rowMid(const AlignCalc::DoubleMatrixR& m, int i)
        {
            return {m(i, ColMX), m(i, ColMY), m(i, ColMZ)};
        }
        Eigen::Vector3d rowDir(const AlignCalc::DoubleMatrixR& m, int i)
        {
            return Eigen::Vector3d(m(i, ColDX), m(i, ColDY),
                                   m(i, ColDZ))
                .normalized();
        }

        struct StriationRules : AlignCalc::LabelRules
        {
            double maxLengthRatio = 0.0;
            // > 0: drop edges whose two lines imply a rotation beyond
            // this (radians); angleTol / sepTol size the noise margin
            double maxRotation = 0.0;
            double angleTol = 0.0, sepTol = 0.0;

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

            // Two lines fix a frame on each side, parallel or not:
            // their mean (signed) direction d, the perpendicular offset
            // p between their midpoints, and d x p. The rotation taking
            // the left frame to the right one estimates the
            // transform's; an edge of an alignment within maxRotation
            // stays within it up to the noise in d (angleTol) and in
            // p's direction (asin(2 sepTol / |p|)). Rejects end-for-end
            // flips, which the distance/angle tests cannot tell apart.
            template <class Invariants>
            bool edgeOk(const AlignCalc::DoubleMatrixR& L, int32_t i1,
                        int32_t j1, const AlignCalc::DoubleMatrixR& R,
                        int32_t i2, int32_t j2, const Invariants&) const
            {
                if (!(maxRotation > 0.0)) return true;
                Eigen::Matrix3d F[2];
                double plen = 0.0;
                for (int s = 0; s < 2; ++s)
                {
                    const AlignCalc::DoubleMatrixR& m = s ? R : L;
                    const int a = s ? i2 : i1, b = s ? j2 : j1;
                    const Eigen::Vector3d d =
                        (rowDir(m, a) + rowDir(m, b)).normalized();
                    const Eigen::Vector3d dm =
                        rowMid(m, b) - rowMid(m, a);
                    const Eigen::Vector3d p = dm - dm.dot(d) * d;
                    const double n = p.norm();
                    if (!(n > 0.0)) return true;
                    plen = s ? std::min(plen, n) : n;
                    F[s].col(0) = d;
                    F[s].col(1) = p / n;
                    F[s].col(2) = d.cross(p / n);
                }
                const double margin =
                    angleTol +
                    std::asin(std::min(1.0, 2.0 * sepTol / plen));
                if (maxRotation + margin >= kPi) return true;
                const Eigen::Matrix3d Rot = F[1] * F[0].transpose();
                const double c =
                    std::clamp((Rot.trace() - 1.0) * 0.5, -1.0, 1.0);
                return std::acos(c) <= maxRotation + margin;
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

        // Whether |T| maps left line |l| onto right line |r|: their
        // directions agree within |angleTol| and the mapped left
        // midpoint lies within |sepTol| of the right line (the offset
        // along the line is not checked; it is often weakly fixed).
        bool explains(const Eigen::Isometry3d& T,
                      const AlignCalc::DoubleMatrixR& L, int l,
                      const AlignCalc::DoubleMatrixR& R, int r,
                      double angleTol, double sepTol)
        {
            const Eigen::Vector3d dl = T.linear() * rowDir(L, l);
            const Eigen::Vector3d dr = rowDir(R, r);
            const double c = std::min(1.0, std::abs(dl.dot(dr)));
            if (std::acos(c) > angleTol) return false;
            const Eigen::Vector3d q = T * rowMid(L, l) - rowMid(R, r);
            return (q - q.dot(dr) * dr).norm() <= sepTol;
        }

        struct LineFitter
        {
            const AlignCalc::DoubleMatrixR& left;
            const AlignCalc::DoubleMatrixR& right;
            const AlignCalc::LineCols& cols;
            double angleTol, sepTol, maxRot;
            std::vector<StriationAlignment>& out;
            StriationAlignment cur;

            bool fit(const std::vector<AlignCalc::Correspondence>& clq)
            {
                AlignCalc::LineFit fit;
                if (!AlignCalc::estimateRigidLines3D(left, right, cols,
                                                     clq, fit))
                    return false;
                cur = StriationAlignment{};
                cur.leftToRight = fit.transform;
                cur.axis = fit.axis;
                cur.rmsPerp = fit.rmsPerp;
                cur.axialWeak = fit.axialWeak;
                for (const auto& c : clq)
                    cur.pairs.push_back({c.left, c.right});
                return true;
            }
            bool explains(AlignCalc::Correspondence c) const
            {
                return shoecomp::explains(cur.leftToRight, left, c.left,
                                          right, c.right, angleTol,
                                          sepTol);
            }
            // a transform rotating past maxRot (e.g. one land reversed
            // end-for-end) is set aside but not reported
            bool accept() const
            {
                return !(maxRot > 0.0 &&
                         Eigen::AngleAxisd(cur.leftToRight.rotation())
                                 .angle() > maxRot);
            }
            void keep(int32_t explained)
            {
                cur.explained = explained;
                out.push_back(std::move(cur));
            }
        };
    }  // namespace

    std::vector<StriationLine> surfaceStriations(
        const Surface3D& surf, std::vector<int>* peakIndex)
    {
        std::vector<StriationLine> out;
        if (peakIndex) peakIndex->clear();
        const jt::Json& ann = surf.annotations;
        if (!ann.isObject() || !ann.contains("peaks")) return out;
        const jt::Json& peaks = ann["peaks"];
        if (!peaks.isArray()) return out;
        const auto& arr = peaks.getArray();
        for (size_t k = 0; k < arr.size(); ++k)
        {
            StriationLine ln;
            if (!striationLineFromJson(arr[k], ln)) continue;
            out.push_back(ln);
            if (peakIndex) peakIndex->push_back((int)k);
        }
        return out;
    }

    AlignCalc::DoubleMatrixR striationRows(
        const std::vector<StriationLine>& lines)
    {
        AlignCalc::DoubleMatrixR m((Eigen::Index)lines.size(),
                                   NumStriationCols);
        // Directions all point along the land's dominant direction
        // (its largest component positive), so the rotation check in
        // StriationRules::edgeOk can use their sign.
        Eigen::Matrix3d scatter = Eigen::Matrix3d::Zero();
        for (const StriationLine& s : lines)
            scatter +=
                s.dir.normalized() * s.dir.normalized().transpose();
        Eigen::Vector3d axis =
            Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d>(scatter)
                .eigenvectors()
                .col(2);
        Eigen::Index big;
        axis.cwiseAbs().maxCoeff(&big);
        if (axis(big) < 0.0) axis = -axis;
        for (size_t i = 0; i < lines.size(); ++i)
        {
            const StriationLine& s = lines[i];
            const Eigen::Vector3d mid =
                s.start + 0.5 * s.length * s.dir;
            const Eigen::Vector3d dir =
                s.dir.dot(axis) < 0.0 ? Eigen::Vector3d(-s.dir) : s.dir;
            const Eigen::Index r = (Eigen::Index)i;
            m(r, ColMX) = mid.x();
            m(r, ColMY) = mid.y();
            m(r, ColMZ) = mid.z();
            m(r, ColDX) = dir.x();
            m(r, ColDY) = dir.y();
            m(r, ColDZ) = dir.z();
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
        lp.extentScale = params.extentScale;
        StriationRules rules;
        rules.maxLengthRatio = params.maxLengthRatio;
        rules.maxRotation = params.maxRotationDeg * kPi / 180.0;
        rules.angleTol = lp.angleTol;
        rules.sepTol = lp.sepTol;
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

        AlignCalc::DistinctParams dp;
        dp.numWorkers = params.numWorkers;
        dp.numResults = params.numResults;
        dp.lowerBound = minLines;
        dp.upperBound = (int32_t)std::min(left.rows(), right.rows());
        if (params.maxLines > 0)
            dp.upperBound = std::min(dp.upperBound, params.maxLines);
        dp.mode = params.distinct;
        dp.minFrac = params.distinctMinFrac;
        LineFitter fitter{
            left,        right,     cols,
            lp.angleTol, lp.sepTol, params.maxRotationDeg * kPi / 180.0,
            out,         {}};
        channel.report(0.5f, "searching consistent matches...");
        switch (AlignCalc::searchDistinct(graph, dp, fitter, channel))
        {
            case AlignCalc::DistinctStatus::Ok:
                return true;
            case AlignCalc::DistinctStatus::FitFailed:
                channel.error("transform fit failed");
                return false;
            case AlignCalc::DistinctStatus::Cancelled:
                return false;
            default:
                channel.error(kNoStriationMatch);
                return false;
        }
    }
}  // namespace shoecomp
