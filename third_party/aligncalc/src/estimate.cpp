#include "aligncalc/estimate.h"
#include "Eigen/Dense"
#include <algorithm>
#include <cmath>
#include <limits>

namespace AlignCalc
{
    namespace
    {
        void centerify(DoubleMatrixR& pts, Eigen::Vector2d& center)
        {
            center(0) = pts.col(0).mean();
            center(1) = pts.col(1).mean();
            pts.col(0).array() -= center(0);
            pts.col(1).array() -= center(1);
        }

        // 2-D Umeyama through the rotation angle (the 2-D results
        // depend on its exact rounding).
        bool similarity2D(const DoubleMatrixR& left,
                          const DoubleMatrixR& right,
                          const PointCols& cols,
                          const std::vector<Correspondence>& pairs,
                          bool withScale, PointTransform& out)
        {
            const Eigen::Index n = (Eigen::Index)pairs.size();
            DoubleMatrixR src(n, 2), dst(n, 2);
            for (Eigen::Index i = 0; i < n; ++i)
                for (int d = 0; d < 2; ++d)
                {
                    src(i, d) = left(pairs[i].left, cols.col(d));
                    dst(i, d) = right(pairs[i].right, cols.col(d));
                }
            Eigen::Vector2d srcMean, dstMean;
            centerify(dst, dstMean);
            centerify(src, srcMean);
            const double srcVariance =
                src.rowwise().norm().array().square().mean();
            auto H = (dst.transpose() * src) / (1.0f * n);
            Eigen::JacobiSVD<DoubleMatrixR,
                             Eigen::ComputeFullU | Eigen::ComputeFullV>
                svd(H);
            auto U = svd.matrixU();
            Eigen::VectorXd d = svd.singularValues();
            auto Vt = svd.matrixV().transpose();
            Eigen::MatrixXd S = Eigen::MatrixXd::Identity(2, 2);
            const double det = U.determinant() * Vt.determinant();
            S(1, 1) *= (det >= 0 ? 1 : -1);

            const double scale = withScale
                                     ? (d.asDiagonal() * S).trace() /
                                           (srcVariance + 1e-8)
                                     : 1.0;
            const Eigen::Matrix2d rotmat = (U * S * Vt).transpose();
            const double rot = std::atan2(rotmat(0, 1), rotmat(0, 0));
            const double c = std::cos(rot), s = std::sin(rot);
            out.angle = rot;
            out.scale = scale;
            out.rotation.resize(2, 2);
            out.rotation << c, -s, s, c;
            out.translation.resize(2);
            out.translation(0) =
                dstMean(0) - scale * (srcMean(0) * c - srcMean(1) * s);
            out.translation(1) =
                dstMean(1) - scale * (srcMean(0) * s + srcMean(1) * c);
            return std::isfinite(scale) && std::isfinite(rot) &&
                   out.translation.allFinite();
        }

        // Umeyama in any dimension but 2.
        bool similarityND(const DoubleMatrixR& left,
                          const DoubleMatrixR& right,
                          const PointCols& cols,
                          const std::vector<Correspondence>& pairs,
                          bool withScale, PointTransform& out)
        {
            const int dim = cols.dim;
            const Eigen::Index n = (Eigen::Index)pairs.size();
            Eigen::MatrixXd X(dim, n), Y(dim, n);
            for (Eigen::Index i = 0; i < n; ++i)
                for (int d = 0; d < dim; ++d)
                {
                    X(d, i) = left(pairs[i].left, cols.col(d));
                    Y(d, i) = right(pairs[i].right, cols.col(d));
                }
            const Eigen::VectorXd mx = X.rowwise().mean();
            const Eigen::VectorXd my = Y.rowwise().mean();
            X.colwise() -= mx;
            Y.colwise() -= my;
            const Eigen::MatrixXd sigma = Y * X.transpose() / double(n);
            Eigen::JacobiSVD<Eigen::MatrixXd> svd(
                sigma, Eigen::ComputeFullU | Eigen::ComputeFullV);
            Eigen::VectorXd S = Eigen::VectorXd::Ones(dim);
            if (svd.matrixU().determinant() *
                    svd.matrixV().determinant() <
                0)
                S(dim - 1) = -1.0;
            out.rotation = svd.matrixU() * S.asDiagonal() *
                           svd.matrixV().transpose();
            const double varX = X.squaredNorm() / double(n);
            out.scale =
                withScale ? svd.singularValues().dot(S) / varX : 1.0;
            out.translation = my - out.scale * out.rotation * mx;
            out.angle = 0.0;
            return std::isfinite(out.scale) &&
                   out.rotation.allFinite() &&
                   out.translation.allFinite();
        }

        Eigen::Vector3d col3(const DoubleMatrixR& m, int32_t r,
                             const std::array<int, 3>& c)
        {
            return {m(r, c[0]), m(r, c[1]), m(r, c[2])};
        }

        Eigen::Matrix3d skew(const Eigen::Vector3d& v)
        {
            Eigen::Matrix3d s;
            s << 0, -v.z(), v.y(), v.z(), 0, -v.x(), -v.y(), v.x(), 0;
            return s;
        }

        Eigen::Isometry3d kabsch(
            const std::vector<Eigen::Vector3d>& src,
            const std::vector<Eigen::Vector3d>& dst)
        {
            Eigen::Vector3d cs = Eigen::Vector3d::Zero();
            Eigen::Vector3d cd = Eigen::Vector3d::Zero();
            for (size_t k = 0; k < src.size(); ++k)
            {
                cs += src[k];
                cd += dst[k];
            }
            cs /= double(src.size());
            cd /= double(dst.size());
            Eigen::Matrix3d H = Eigen::Matrix3d::Zero();
            for (size_t k = 0; k < src.size(); ++k)
                H += (src[k] - cs) * (dst[k] - cd).transpose();
            Eigen::JacobiSVD<Eigen::Matrix3d> svd(
                H, Eigen::ComputeFullU | Eigen::ComputeFullV);
            Eigen::Matrix3d D = Eigen::Matrix3d::Identity();
            if ((svd.matrixV() * svd.matrixU().transpose())
                    .determinant() < 0)
                D(2, 2) = -1;
            Eigen::Isometry3d T = Eigen::Isometry3d::Identity();
            T.linear() = svd.matrixV() * D * svd.matrixU().transpose();
            T.translation() = cd - T.linear() * cs;
            return T;
        }
    }  // namespace

    bool estimatePointTransform(
        const DoubleMatrixR& left, const DoubleMatrixR& right,
        const PointCols& cols, const std::vector<Correspondence>& pairs,
        bool withScale, PointTransform& out)
    {
        if (!cols.valid(left) || !cols.valid(right) || pairs.size() < 2)
            return false;
        return cols.dim == 2 ? similarity2D(left, right, cols, pairs,
                                            withScale, out)
                             : similarityND(left, right, cols, pairs,
                                            withScale, out);
    }

    bool estimateRigid3D(const DoubleMatrixR& left,
                         const DoubleMatrixR& right,
                         const PointCols& cols,
                         const std::vector<Correspondence>& pairs,
                         Eigen::Isometry3d& out)
    {
        if (cols.dim != 3 || !cols.valid(left) || !cols.valid(right) ||
            pairs.size() < 3)
            return false;
        std::vector<Eigen::Vector3d> src, dst;
        for (const auto& c : pairs)
        {
            src.push_back({left(c.left, cols.col(0)),
                           left(c.left, cols.col(1)),
                           left(c.left, cols.col(2))});
            dst.push_back({right(c.right, cols.col(0)),
                           right(c.right, cols.col(1)),
                           right(c.right, cols.col(2))});
        }
        out = kabsch(src, dst);
        return true;
    }

    bool estimateRigidLines3D(const DoubleMatrixR& left,
                              const DoubleMatrixR& right,
                              const LineCols& cols,
                              const std::vector<Correspondence>& pairs,
                              LineFit& out)
    {
        const size_t n = pairs.size();
        if (!cols.valid(left) || !cols.valid(right) || n < 2)
            return false;

        std::vector<Eigen::Vector3d> mL(n), dL(n), mR(n), dR(n);
        std::vector<double> lens;
        for (size_t k = 0; k < n; ++k)
        {
            mL[k] = col3(left, pairs[k].left, cols.mid);
            mR[k] = col3(right, pairs[k].right, cols.mid);
            dL[k] = col3(left, pairs[k].left, cols.dir).normalized();
            dR[k] = col3(right, pairs[k].right, cols.dir).normalized();
            if (!dL[k].allFinite() || !dR[k].allFinite()) return false;
            if (k > 0 && dL[k].dot(dL[0]) < 0) dL[k] = -dL[k];
            if (k > 0 && dR[k].dot(dR[0]) < 0) dR[k] = -dR[k];
            lens.push_back(std::fabs(left(pairs[k].left, cols.length)));
        }
        std::nth_element(lens.begin(), lens.begin() + n / 2,
                         lens.end());
        const double lambda = std::max(lens[n / 2], 1e-12);

        Eigen::Vector3d meanL = Eigen::Vector3d::Zero();
        Eigen::Vector3d meanR = Eigen::Vector3d::Zero();
        for (size_t k = 0; k < n; ++k)
        {
            meanL += mL[k];
            meanR += mR[k];
        }
        meanL /= double(n);
        meanR /= double(n);
        const double mu = 1e-3 * std::sqrt(double(n));

        // The bundle's direction signs are only known up to one global
        // flip between left and right; fit both and keep the better.
        double bestCost = std::numeric_limits<double>::infinity();
        for (int sign : {1, -1})
        {
            std::vector<Eigen::Vector3d> d(n);
            Eigen::Vector3d axis = Eigen::Vector3d::Zero();
            for (size_t k = 0; k < n; ++k)
            {
                d[k] = double(sign) * dR[k];
                axis += d[k];
            }
            axis.normalize();

            std::vector<Eigen::Vector3d> src, dst;
            for (size_t k = 0; k < n; ++k)
            {
                src.push_back(mL[k]);
                src.push_back(mL[k] + lambda * dL[k]);
                dst.push_back(mR[k]);
                dst.push_back(mR[k] + lambda * d[k]);
            }
            Eigen::Isometry3d T = kabsch(src, dst);

            double cost = 0.0;
            for (int iter = 0; iter < 30; ++iter)
            {
                Eigen::Matrix<double, 6, 6> H =
                    Eigen::Matrix<double, 6, 6>::Zero();
                Eigen::Matrix<double, 6, 1> g =
                    Eigen::Matrix<double, 6, 1>::Zero();
                const Eigen::Matrix3d R = T.linear();
                cost = 0.0;
                for (size_t k = 0; k < n; ++k)
                {
                    const Eigen::Matrix3d P =
                        Eigen::Matrix3d::Identity() -
                        d[k] * d[k].transpose();
                    const Eigen::Vector3d Rm = R * mL[k];
                    Eigen::Matrix<double, 3, 6> J;
                    J.leftCols<3>() = -P * skew(Rm);
                    J.rightCols<3>() = P;
                    const Eigen::Vector3d r =
                        P * (Rm + T.translation() - mR[k]);
                    H += J.transpose() * J;
                    g += J.transpose() * r;

                    const Eigen::Vector3d Rd = R * dL[k];
                    Eigen::Matrix<double, 3, 6> Jd;
                    Jd.leftCols<3>() = -lambda * skew(Rd);
                    Jd.rightCols<3>().setZero();
                    const Eigen::Vector3d rd = lambda * (Rd - d[k]);
                    H += Jd.transpose() * Jd;
                    g += Jd.transpose() * rd;
                    cost += r.squaredNorm() + rd.squaredNorm();
                }
                const Eigen::Vector3d Rmean = R * meanL;
                Eigen::Matrix<double, 1, 6> Ja;
                Ja.leftCols<3>() = -mu * axis.transpose() * skew(Rmean);
                Ja.rightCols<3>() = mu * axis.transpose();
                const double ra =
                    mu * axis.dot(Rmean + T.translation() - meanR);
                H += Ja.transpose() * Ja;
                g += Ja.transpose() * ra;

                const Eigen::Matrix<double, 6, 1> delta =
                    H.ldlt().solve(-g);
                if (!delta.allFinite()) break;
                const Eigen::Vector3d w = delta.head<3>();
                const double angle = w.norm();
                if (angle > 0)
                    T.linear() = Eigen::AngleAxisd(angle, w / angle)
                                     .toRotationMatrix() *
                                 T.linear();
                T.translation() += delta.tail<3>();
                if (angle < 1e-12 && delta.tail<3>().norm() < 1e-15)
                    break;
            }
            if (!(cost < bestCost)) continue;
            bestCost = cost;

            double sumSq = 0.0;
            double axialInfo = 0.0;
            for (size_t k = 0; k < n; ++k)
            {
                const Eigen::Matrix3d P = Eigen::Matrix3d::Identity() -
                                          d[k] * d[k].transpose();
                sumSq += (P * (T * mL[k] - mR[k])).squaredNorm();
                axialInfo += (P * axis).squaredNorm();
            }
            out.transform = T;
            out.axis = axis;
            out.rmsPerp = std::sqrt(sumSq / double(n));
            // mean sin^2 of the angle between the lines and the axis
            const double s2 =
                std::sin(2.0 * 3.14159265358979323846 / 180);
            out.axialWeak = axialInfo / double(n) < s2 * s2;
        }
        return std::isfinite(bestCost);
    }

}  // namespace AlignCalc
