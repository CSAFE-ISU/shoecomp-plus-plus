#include "aligncalc/lineAnchor3d.h"
#include "Eigen/Geometry"
#include <algorithm>
#include <cmath>

namespace AlignCalc
{
    namespace
    {
        void rows(const DoubleMatrixR& m, const LineCols& c,
                  std::vector<Eigen::Vector3d>& mid,
                  std::vector<Eigen::Vector3d>& dir)
        {
            mid.resize(m.rows());
            dir.resize(m.rows());
            for (Eigen::Index i = 0; i < m.rows(); ++i)
            {
                mid[i] = {m(i, c.mid[0]), m(i, c.mid[1]),
                          m(i, c.mid[2])};
                dir[i] = Eigen::Vector3d(m(i, c.dir[0]), m(i, c.dir[1]),
                                         m(i, c.dir[2]))
                             .normalized();
            }
        }

        // Frame of a line pair: mean direction, perpendicular midpoint
        // offset, their cross product. False if the offset vanishes.
        bool frame(const Eigen::Vector3d& ma, const Eigen::Vector3d& da,
                   const Eigen::Vector3d& mb, Eigen::Vector3d db,
                   Eigen::Matrix3d& F, double& plen)
        {
            if (da.dot(db) < 0) db = -db;
            const Eigen::Vector3d d = (da + db).normalized();
            const Eigen::Vector3d w = mb - ma;
            const Eigen::Vector3d p = w - w.dot(d) * d;
            plen = p.norm();
            if (!(plen > 0.0) || !d.allFinite()) return false;
            F.col(0) = d;
            F.col(1) = p / plen;
            F.col(2) = d.cross(F.col(1));
            return true;
        }
    }  // namespace

    LineAnchor3D::LineAnchor3D(const DoubleMatrixR& left,
                               const DoubleMatrixR& right,
                               const LineCols& cols, double acrossTol,
                               double depthTol)
        : acrossTol_(acrossTol), depthTol_(depthTol)
    {
        if (cols.valid(left) && cols.valid(right))
        {
            rows(left, cols, midL_, dirL_);
            rows(right, cols, midR_, dirR_);
        }
    }

    void LineAnchor3D::filter(Correspondence a, Correspondence b,
                              const Correspondence* cands, size_t n,
                              uint8_t* keep) const
    {
        std::fill(keep, keep + n, uint8_t(0));
        if (midL_.empty() || midR_.empty()) return;
        Eigen::Matrix3d FL, FR;
        double pL, pR;
        if (!frame(midL_[a.left], dirL_[a.left], midL_[b.left],
                   dirL_[b.left], FL, pL) ||
            !frame(midR_[a.right], dirR_[a.right], midR_[b.right],
                   dirR_[b.right], FR, pR))
            return;
        const Eigen::Vector3d cL =
            0.5 * (midL_[a.left] + midL_[b.left]);
        const Eigen::Vector3d cR =
            0.5 * (midR_[a.right] + midR_[b.right]);

        // sign -1: the right directions run the other way, a rotation
        // by 180 deg about p (flips d and d x p)
        thread_local std::vector<uint8_t> other;
        other.resize(n);
        size_t kept[2] = {0, 0};
        for (int s = 0; s < 2; ++s)
        {
            Eigen::Matrix3d F = FR;
            if (s)
            {
                F.col(0) = -F.col(0);
                F.col(2) = -F.col(2);
            }
            const Eigen::Matrix3d rot = F * FL.transpose();
            const Eigen::Vector3d t = cR - rot * cL;
            const Eigen::Vector3d e2 = F.col(1), e3 = F.col(2);
            uint8_t* out = s ? other.data() : keep;
            for (size_t k = 0; k < n; ++k)
            {
                const int32_t l = cands[k].left, r = cands[k].right;
                const Eigen::Vector3d& dr = dirR_[r];
                Eigen::Vector3d q = rot * midL_[l] + t - midR_[r];
                q -= q.dot(dr) * dr;
                out[k] = std::fabs(q.dot(e2)) <= acrossTol_ &&
                         std::fabs(q.dot(e3)) <= depthTol_;
                kept[s] += out[k];
            }
        }
        if (kept[1] > kept[0])
            std::copy(other.begin(), other.begin() + n, keep);
    }
}  // namespace AlignCalc
