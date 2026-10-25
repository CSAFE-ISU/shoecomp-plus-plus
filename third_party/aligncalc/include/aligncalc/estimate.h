#ifndef ALIGNCALC_ESTIMATE_H
#define ALIGNCALC_ESTIMATE_H
#include "Eigen/Geometry"
#include "correspondence.h"
#include "rigidLines3d.h"
#include "rules.h"

namespace AlignCalc
{
    // x -> scale * rotation * x + translation, in any dimension
    // (rotation dim x dim, no reflection).
    struct PointTransform
    {
        Eigen::MatrixXd rotation;
        double scale = 1.0;
        Eigen::VectorXd translation;
        double angle = 0.0;  // 2-D: the rotation angle (radians)
    };

    // Similarity (|withScale|) or rigid transform T with
    // T(left) ~= right over the matched rows, least squares
    // (Umeyama), in cols.dim dimensions. Needs >= 2 pairs; false when
    // the fit is not finite.
    ALIGNCALC_API bool estimatePointTransform(
        const DoubleMatrixR& left, const DoubleMatrixR& right,
        const PointCols& cols, const std::vector<Correspondence>& pairs,
        bool withScale, PointTransform& out);

    // Rigid transform T with T * left ~= right over the matched rows
    // (Kabsch, least squares). |cols| must be 3-D; needs >= 3 pairs.
    ALIGNCALC_API bool estimateRigid3D(
        const DoubleMatrixR& left, const DoubleMatrixR& right,
        const PointCols& cols, const std::vector<Correspondence>& pairs,
        Eigen::Isometry3d& out);

    struct LineFit
    {
        Eigen::Isometry3d transform = Eigen::Isometry3d::Identity();
        Eigen::Vector3d axis = Eigen::Vector3d::UnitZ();  // right frame
        double rmsPerp = 0.0;  // midpoint-to-line distance, right frame
        bool axialWeak = false;  // translation along |axis| comes from
                                 // matched midpoints only
    };

    // Rigid transform T with T * left ~= right from matched line
    // segments: minimizes the distance of each transformed left
    // midpoint to its right line plus direction misalignment. Matched
    // lines that are (nearly) parallel cannot fix the translation along
    // their direction; it is then taken from the matched midpoints and
    // |axialWeak| is set. Directions may have either sign but the
    // matched lines are assumed to form a near-parallel bundle.
    // Needs >= 2 pairs.
    ALIGNCALC_API bool estimateRigidLines3D(
        const DoubleMatrixR& left, const DoubleMatrixR& right,
        const LineCols& cols, const std::vector<Correspondence>& pairs,
        LineFit& out);

}  // namespace AlignCalc

#endif
