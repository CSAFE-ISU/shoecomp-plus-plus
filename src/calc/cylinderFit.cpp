#include "calc/cylinderFit.h"

#include <cmath>
#include <vector>

#include "aligncalc/eigen/Eigen/Eigenvalues"

namespace shoecomp
{
    namespace
    {
        constexpr double kPi = 3.14159265358979323846;

        double wrapPi(double a)
        {
            while (a > kPi) a -= 2.0 * kPi;
            while (a < -kPi) a += 2.0 * kPi;
            return a;
        }

        bool physAt(const Surface3D& s, int r, int c,
                    Eigen::Vector3d& p)
        {
            float zv = s.z(r, c);
            if (!std::isfinite(zv)) return false;
            p = Eigen::Vector3d(s.px(c), s.py(r), s.pz(zv));
            return true;
        }
    }  // namespace

    bool fitCylinder(const Surface3D& surf, CylinderFit& out,
                     std::string& err)
    {
        out = CylinderFit{};
        const int X = surf.sizeX, Y = surf.sizeY;
        if (X < 3 || Y < 3 || surf.z.size() == 0)
        {
            err = "surface too small to fit a cylinder";
            return false;
        }

        // Cap the working set so large surfaces (~1M cells) stay cheap.
        long total = (long)X * Y;
        int stride =
            (int)std::max(1.0, std::sqrt((double)total / 50000.0));

        // Axis = direction that all surface normals are most nearly
        // perpendicular to = smallest-eigenvalue eigenvector of the
        // normal covariance M = sum(n nT).
        Eigen::Matrix3d M = Eigen::Matrix3d::Zero();
        std::vector<Eigen::Vector3d> pts;
        Eigen::Vector3d centroid = Eigen::Vector3d::Zero();
        long nNorm = 0;

        for (int r = 1; r + 1 < Y; r += stride)
        {
            for (int c = 1; c + 1 < X; c += stride)
            {
                Eigen::Vector3d Pc, Pl, Pr, Pu, Pd;
                if (!physAt(surf, r, c, Pc)) continue;
                pts.push_back(Pc);
                centroid += Pc;
                if (!physAt(surf, r, c - 1, Pl)) Pl = Pc;
                if (!physAt(surf, r, c + 1, Pr)) Pr = Pc;
                if (!physAt(surf, r - 1, c, Pu)) Pu = Pc;
                if (!physAt(surf, r + 1, c, Pd)) Pd = Pc;
                Eigen::Vector3d n = (Pr - Pl).cross(Pd - Pu);
                double len = n.norm();
                if (len > 1e-30)
                {
                    n /= len;
                    M += n * n.transpose();
                    ++nNorm;
                }
            }
        }
        if (pts.size() < 16 || nNorm < 16)
        {
            err = "not enough valid samples to fit a cylinder";
            return false;
        }
        centroid /= (double)pts.size();

        Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> es(M);
        if (es.info() != Eigen::Success)
        {
            err = "cylinder axis solve failed";
            return false;
        }
        Eigen::Vector3d axis = es.eigenvectors().col(0).normalized();

        // Cross-section basis (e1, e2) perpendicular to axis.
        Eigen::Vector3d ref = Eigen::Vector3d::UnitX();
        if (std::fabs(axis.x()) >= std::fabs(axis.y()) &&
            std::fabs(axis.x()) >= std::fabs(axis.z()))
            ref = Eigen::Vector3d::UnitY();
        Eigen::Vector3d e1 = (ref - axis * ref.dot(axis)).normalized();
        Eigen::Vector3d e2 = axis.cross(e1).normalized();

        // Algebraic (Kasa) circle fit in the (e1,e2) plane, centered at
        // the centroid so sum(u)=sum(v)=0.
        double Suu = 0, Svv = 0, Suv = 0;
        double Suuu = 0, Svvv = 0, Suvv = 0, Svuu = 0;
        for (const Eigen::Vector3d& p : pts)
        {
            Eigen::Vector3d w = p - centroid;
            double u = w.dot(e1), v = w.dot(e2);
            Suu += u * u;
            Svv += v * v;
            Suv += u * v;
            Suuu += u * u * u;
            Svvv += v * v * v;
            Suvv += u * v * v;
            Svuu += v * u * u;
        }
        Eigen::Matrix2d A;
        A << Suu, Suv, Suv, Svv;
        Eigen::Vector2d rhs(0.5 * (Suuu + Suvv), 0.5 * (Svvv + Svuu));
        Eigen::Vector2d uc = A.ldlt().solve(rhs);
        double R2 = uc.x() * uc.x() + uc.y() * uc.y() +
                    (Suu + Svv) / (double)pts.size();
        if (!(R2 > 0.0))
        {
            err = "degenerate circle fit (surface not curved enough)";
            return false;
        }
        // Kasa is biased for short arcs; refine to the geometric
        // best-fit circle (minimize sum (r_i - R)^2) with a few
        // Gauss-Newton steps seeded from the algebraic solution.
        double a = uc.x(), b = uc.y(), R = std::sqrt(R2);
        for (int it = 0; it < 30; ++it)
        {
            Eigen::Matrix3d JtJ = Eigen::Matrix3d::Zero();
            Eigen::Vector3d Jtr = Eigen::Vector3d::Zero();
            for (const Eigen::Vector3d& p : pts)
            {
                Eigen::Vector3d w = p - centroid;
                double du = w.dot(e1) - a, dv = w.dot(e2) - b;
                double Li = std::sqrt(du * du + dv * dv);
                if (Li < 1e-30) continue;
                double res = Li - R;
                Eigen::Vector3d J(-du / Li, -dv / Li, -1.0);
                JtJ += J * J.transpose();
                Jtr += J * res;
            }
            Eigen::Vector3d d = JtJ.ldlt().solve(-Jtr);
            a += d(0);
            b += d(1);
            R += d(2);
            if (d.norm() < 1e-12) break;
        }
        Eigen::Vector3d center = centroid + a * e1 + b * e2;
        if (!std::isfinite(R) || R <= 0.0)
        {
            err = "invalid cylinder radius";
            return false;
        }

        // theta origin = circular mean of the sample angles, so the arc
        // is centered near 0.
        double sc = 0, ss = 0;
        for (const Eigen::Vector3d& p : pts)
        {
            Eigen::Vector3d d = p - center;
            Eigen::Vector3d rho = d - d.dot(axis) * axis;
            double th = std::atan2(rho.dot(e2), rho.dot(e1));
            sc += std::cos(th);
            ss += std::sin(th);
        }
        double thetaOrigin = std::atan2(ss, sc);

        double sMin = 1e300, sMax = -1e300;
        double tMin = 1e300, tMax = -1e300;
        for (const Eigen::Vector3d& p : pts)
        {
            Eigen::Vector3d d = p - center;
            double sax = d.dot(axis);
            Eigen::Vector3d rho = d - sax * axis;
            double adj = wrapPi(std::atan2(rho.dot(e2), rho.dot(e1)) -
                                thetaOrigin);
            sMin = std::min(sMin, sax);
            sMax = std::max(sMax, sax);
            tMin = std::min(tMin, adj);
            tMax = std::max(tMax, adj);
        }

        out.valid = true;
        out.axis = axis;
        out.center = center;
        out.radius = R;
        out.e1 = e1;
        out.e2 = e2;
        out.thetaOrigin = thetaOrigin;
        out.sMin = sMin;
        out.sMax = sMax;
        out.thetaMin = tMin;
        out.thetaMax = tMax;
        return true;
    }

    void toCylindrical(const CylinderFit& fit, const Eigen::Vector3d& p,
                       double& theta, double& s, double& r)
    {
        Eigen::Vector3d d = p - fit.center;
        s = d.dot(fit.axis);
        Eigen::Vector3d rho = d - s * fit.axis;
        r = rho.norm();
        theta = wrapPi(std::atan2(rho.dot(fit.e2), rho.dot(fit.e1)) -
                       fit.thetaOrigin);
    }
}  // namespace shoecomp
