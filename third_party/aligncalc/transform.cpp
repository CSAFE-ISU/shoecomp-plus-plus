#include "aligncalc.internal.h"
#include "Eigen/SVD"
#include <cmath>

namespace AlignCalc
{
    static void centerify(DoubleMatrixR &pts, Eigen::Vector2d &center)
    {
        center(0) = pts.col(0).mean();
        center(1) = pts.col(1).mean();
        pts.col(0).array() -= center(0);
        pts.col(1).array() -= center(1);
    }

    void RTSTransform::fromMatrix(const DoubleMatrixR &mat)
    {
        Eigen::Matrix2d rotmat = mat.block<2, 2>(1, 0);
        this->dx = mat(0, 0);
        this->dy = mat(0, 1);
        this->scale = std::sqrt(rotmat.determinant());
        this->rotation = std::atan2(rotmat(0, 1), rotmat(0, 0));
    }

    void RTSTransform::toMatrix(DoubleMatrixR &mat) const
    {
        mat.resize(3, 2);
        mat(0, 0) = dx;
        mat(0, 1) = dy;
        mat(1, 0) = scale * std::cos(rotation);
        mat(1, 1) = scale * std::sin(rotation);
        mat(2, 0) = scale * -std::sin(rotation);
        mat(2, 1) = scale * std::cos(rotation);
    }

    void RTSTransform::estimate(const MatchedPoints &match,
                                bool right2left)
    {
        /* kabsch algorithm to get rotation & translation
         * https://en.wikipedia.org/wiki/Kabsch_algorithm
         * following Umeyama's variant to calculate scale */
        DoubleMatrixR src;
        Eigen::Vector2d srcMean;
        double srcVariance;
        //
        DoubleMatrixR dst;
        Eigen::Vector2d dstMean;
        double dstVariance;
        if (right2left)
        {
            /* we want to estimate the transformation T, such that
             * T(right_pts) = left_pts */
            dst = match.left_pts.leftCols<2>();
            src = match.right_pts.leftCols<2>();
        }
        else
        {
            /* we want to estimate the transformation T, such that
             * T(left_pts) = right_pts */
            src = match.left_pts.leftCols<2>();
            dst = match.right_pts.leftCols<2>();
        }
        centerify(dst, dstMean);
        dstVariance = src.rowwise().norm().array().square().mean();
        centerify(src, srcMean);
        srcVariance = src.rowwise().norm().array().square().mean();
        //
        auto H = (dst.transpose() * src) / (1.0f * match.N);
        Eigen::JacobiSVD<DoubleMatrixR,
                         Eigen::ComputeFullU | Eigen::ComputeFullV>
            svd(H);

        auto U = svd.matrixU();
        Eigen::VectorXd d = svd.singularValues();
        auto Vt = svd.matrixV().transpose();

        Eigen::MatrixXd S = Eigen::MatrixXd::Identity(2, 2);
        double det = U.determinant() * Vt.determinant();
        S(1, 1) *= (det >= 0 ? 1 : -1);

        this->scale =
            (d.asDiagonal() * S).trace() / (srcVariance + 1e-8);
        Eigen::Matrix2d rotmat = (U * S * Vt).transpose();
        rotmat *= this->scale;

        this->rotation = std::atan2(rotmat(0, 1), rotmat(0, 0));
        Eigen::VectorXd shift = -1 * (rotmat * srcMean) + dstMean;
        this->dx = shift(0);
        this->dy = shift(1);
    }

} /* namespace AlignCalc */
