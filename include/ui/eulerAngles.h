#ifndef SHOECOMP_UI_EULER_ANGLES_H
#define SHOECOMP_UI_EULER_ANGLES_H

#include <algorithm>
#include <cmath>

#include "aligncalc/eigen/Eigen/Geometry"

namespace shoecomp
{
    // Shared pitch/yaw/roll convention for the 3D viewer and the 3D
    // alignment transform. pitch = X (right), yaw = Y (up), roll = Z
    // (forward). Composed R = Rz(roll) * Ry(yaw) * Rx(pitch). Angles in
    // degrees. See .claude/x3p-notes.md for rationale.
    inline Eigen::Matrix3f eulerToMatrix(float pitchDeg, float yawDeg,
                                         float rollDeg)
    {
        const float d2r = 3.14159265358979323846f / 180.0f;
        return (Eigen::AngleAxisf(rollDeg * d2r,
                                  Eigen::Vector3f::UnitZ()) *
                Eigen::AngleAxisf(yawDeg * d2r,
                                  Eigen::Vector3f::UnitY()) *
                Eigen::AngleAxisf(pitchDeg * d2r,
                                  Eigen::Vector3f::UnitX()))
            .toRotationMatrix();
    }

    // Inverse of eulerToMatrix: decompose a rotation matrix into
    // (pitch, yaw, roll) degrees under the same convention. For mapping
    // a registration/ICP rotation back onto the UI controls. Degenerate
    // at yaw = +/-90 (gimbal lock), where roll is folded into pitch.
    inline Eigen::Vector3f matrixToEuler(const Eigen::Matrix3f& R)
    {
        const float r2d = 180.0f / 3.14159265358979323846f;
        float yaw = std::asin(std::clamp(-R(2, 0), -1.0f, 1.0f));
        float pitch = std::atan2(R(2, 1), R(2, 2));
        float roll = std::atan2(R(1, 0), R(0, 0));
        return Eigen::Vector3f(pitch * r2d, yaw * r2d, roll * r2d);
    }
}  // namespace shoecomp

#endif
