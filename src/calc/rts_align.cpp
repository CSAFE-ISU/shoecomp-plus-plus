#include "calc/align.h"
#include "aligncalc/alignWithRules.h"
#include "calc/channelProgress.h"
#include "core/pointType.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace shoecomp
{
    enum PointCol : int
    {
        ColX,
        ColY,
        ColType,   // PointType, or kUnlabeled
        ColTheta,  // minutia direction (deg), NaN if none
        NumCols
    };

    constexpr double kUnlabeled = -1.0;

    // EBTS correspondences: an untyped or Other point pairs with any
    // type, otherwise types must be equal. Two correspondences
    // (i1 <-> i2), (j1 <-> j2) must keep the relative direction of
    // their minutiae: theta(i1) - theta(j1) ~ theta(i2) - theta(j2)
    // (mod 360). A rotation shifts all of one side's directions
    // equally, so this holds whatever the angle convention.
    struct EBTSRules : AlignCalc::LabelRules
    {
        double angleTolDeg = 0.0;  // 0 = no direction check

        static bool matchesAny(double t)
        {
            return t == kUnlabeled ||
                   t == static_cast<double>(PointType::Other);
        }

        bool admissible(const AlignCalc::DoubleMatrixR& L, int32_t l,
                        const AlignCalc::DoubleMatrixR& R,
                        int32_t r) const
        {
            const double a = L(l, ColType);
            const double b = R(r, ColType);
            return matchesAny(a) || matchesAny(b) || a == b;
        }

        bool directionsAgree(const AlignCalc::DoubleMatrixR& L,
                             int32_t i1, int32_t j1,
                             const AlignCalc::DoubleMatrixR& R,
                             int32_t i2, int32_t j2) const
        {
            if (!(angleTolDeg > 0.0)) return true;
            const double d = (L(i1, ColTheta) - L(j1, ColTheta)) -
                             (R(i2, ColTheta) - R(j2, ColTheta));
            if (std::isnan(d)) return true;
            return std::fabs(std::remainder(d, 360.0)) <= angleTolDeg;
        }

        template <class Invariants>
        bool edgeOk(const AlignCalc::DoubleMatrixR& L, int32_t i1,
                    int32_t j1, const AlignCalc::DoubleMatrixR& R,
                    int32_t i2, int32_t j2, const Invariants&) const
        {
            return directionsAgree(L, i1, j1, R, i2, j2);
        }

        bool tripleOk(const AlignCalc::DoubleMatrixR& L,
                      const std::array<int32_t, 3>& l,
                      const AlignCalc::DoubleMatrixR& R,
                      const std::array<int32_t, 3>& r) const
        {
            return directionsAgree(L, l[0], l[1], R, r[0], r[1]) &&
                   directionsAgree(L, l[0], l[2], R, r[0], r[2]) &&
                   directionsAgree(L, l[1], l[2], R, r[1], r[2]);
        }
    };

    // EBTS points may lack a type (unlabeled) and carry theta.
    static bool extractAnnotatedPoints(const jt::Json& annotations,
                                       bool ebts,
                                       AlignCalc::DoubleMatrixR& mat)
    {
        const jt::Json& ann = annotations;
        if (!ann.isObject() || !ann.contains("points") ||
            !ann["points"].isArray())
        {
            return false;
        }
        auto& pts = ann["points"].getArray();
        if (pts.size() == 0) { return false; }
        if (pts.size() > AlignCalc::MAX_POINTS) { return false; }
        mat.resize(pts.size(), NumCols);
        for (size_t i = 0; i < pts.size(); ++i)
        {
            auto& el = pts[i];
            if (!el.isObject()) return false;
            if (!el.contains("x") || !el["x"].isNumber()) return false;
            if (!el.contains("y") || !el["y"].isNumber()) return false;
            const bool typed =
                el.contains("type") && el["type"].isString();
            if (!typed && !ebts) return false;
            mat(i, ColX) = el["x"].getNumber();
            mat(i, ColY) = el["y"].getNumber();
            mat(i, ColType) =
                typed ? static_cast<double>(
                            stringToPointType(el["type"].getString()))
                      : kUnlabeled;
            mat(i, ColTheta) =
                ebts && el.contains("theta") && el["theta"].isNumber()
                    ? el["theta"].getNumber()
                    : std::numeric_limits<double>::quiet_NaN();
        }
        return true;
    }

    AlignCalc::Status runRTSAlign(const jt::Json& leftAnn,
                                  const jt::Json& rightAnn,
                                  WorkerChannel& channel,
                                  std::vector<AlignState>& aligns,
                                  const AlignCalc::RTSParams& params,
                                  const PointMatchRules& rules,
                                  std::vector<MatchedRows>* matched,
                                  std::string* message)
    {
        const bool ebts = rules.type == ImageType::EBTS;
        AlignCalc::DoubleMatrixR left_pts;
        AlignCalc::DoubleMatrixR right_pts;
        std::vector<AlignCalc::MatchedPoints> results;
        std::string msg;
        auto fail = [&](AlignCalc::Status status)
        {
            if (status != AlignCalc::Status::Cancelled)
                channel.error(msg.c_str());
            if (message) *message = msg;
            channel.cancelled();
            return status;
        };

        channel.report(0.0f, "loading points...");
        if (!extractAnnotatedPoints(leftAnn, ebts, left_pts))
        {
            msg = "left image does not have 1 to 256 valid points";
            return fail(AlignCalc::Status::Error);
        }
        if (!extractAnnotatedPoints(rightAnn, ebts, right_pts))
        {
            msg = "right image does not have 1 to 256 valid points";
            return fail(AlignCalc::Status::Error);
        }

        AlignCalc::RTSParams run = params;
        if (run.upperBound <= 0)
        {
            run.upperBound = std::max(
                3, (int)std::min(left_pts.rows(), right_pts.rows()));
        }

        AlignCalc::PointCols cols;
        cols.dim = 2;
        cols.pos = {ColX, ColY, 0};
        ChannelProgress progress(channel);
        AlignCalc::Status status;
        if (ebts)
        {
            EBTSRules er;
            er.angleTolDeg = rules.minutiaAngleTolDeg;
            if (er.angleTolDeg > 0.0)
            {
                run.angleCol = ColTheta;
                run.angleTolDeg = er.angleTolDeg;
            }
            status = AlignCalc::getAlignmentWithRules(
                left_pts, right_pts, cols, er, run, progress, results,
                msg);
        }
        else
        {
            status = AlignCalc::getAlignment(left_pts, right_pts, cols,
                                             ColType, run, progress,
                                             results, msg);
        }
        if (status != AlignCalc::Status::Ok) return fail(status);

        AlignCalc::RTSTransform tform;
        for (const auto& match : results)
        {
            tform.estimate(match, true);

            double scale = params.sameScale ? 1.0 : tform.scale;
            if (!params.sameScale && params.scaleRange > 0.0)
            {
                double lo = 1.0 / params.scaleRange;
                double hi = params.scaleRange;
                if (scale < lo || scale > hi) continue;
            }

            aligns.push_back(AlignState{});
            aligns.back().rotation = tform.rotation /* in radians */;
            aligns.back().dx = tform.dx;
            aligns.back().dy = tform.dy;
            aligns.back().scale = scale;
            aligns.back().mode = AlignMode::Automatic;
            if (matched) matched->push_back(match.pairs);

            // Store matched points in info
            aligns.back().info.setObject();
            aligns.back().info["explained"] = jt::Json(match.explained);
            aligns.back().info["leftPoints"].setArray();
            aligns.back().info["rightPoints"].setArray();
            for (int32_t i = 0; i < match.N; ++i)
            {
                jt::Json lpt;
                lpt.setObject();
                lpt["x"] = match.left_pts(i, 0);
                lpt["y"] = match.left_pts(i, 1);
                aligns.back().info["leftPoints"].getArray().push_back(
                    std::move(lpt));

                jt::Json rpt;
                rpt.setObject();
                rpt["x"] = match.right_pts(i, 0);
                rpt["y"] = match.right_pts(i, 1);
                aligns.back().info["rightPoints"].getArray().push_back(
                    std::move(rpt));
            }
        }
        char buf[64];
        snprintf(buf, sizeof(buf), "%zu alignment(s) added",
                 aligns.size());
        channel.report(1.0f, buf);
        channel.done();
        return AlignCalc::Status::Ok;
    }
}  // namespace shoecomp
