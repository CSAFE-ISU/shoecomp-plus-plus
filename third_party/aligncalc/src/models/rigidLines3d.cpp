#include "aligncalc/rigidLines3d.h"
#include "Eigen/Geometry"
#include <algorithm>

namespace AlignCalc
{
    namespace detail
    {
        namespace
        {
            Eigen::Vector3d col3(const DoubleMatrixR& m, int32_t r,
                                 const std::array<int, 3>& c)
            {
                return {m(r, c[0]), m(r, c[1]), m(r, c[2])};
            }

            void pairInvariant(const Eigen::Vector3d& mi,
                               const Eigen::Vector3d& di,
                               const Eigen::Vector3d& mj,
                               Eigen::Vector3d dj, double& angle,
                               double& sep)
            {
                double dot = di.dot(dj);
                if (dot < 0)
                {
                    dj = -dj;
                    dot = -dot;
                }
                angle = std::atan2(di.cross(dj).norm(), dot);
                const Eigen::Vector3d u = (di + dj).normalized();
                const Eigen::Vector3d d = mj - mi;
                sep = (d - d.dot(u) * u).norm();
            }
        }  // namespace

        bool lineInvariants(const DoubleMatrixR& m,
                            const LineCols& cols,
                            std::vector<double>& angle,
                            std::vector<double>& sep)
        {
            if (!cols.valid(m)) return false;
            const int32_t n = int32_t(m.rows());
            std::vector<Eigen::Vector3d> mid(n), dir(n);
            for (int32_t i = 0; i < n; ++i)
            {
                mid[i] = col3(m, i, cols.mid);
                dir[i] = col3(m, i, cols.dir);
                const double norm = dir[i].norm();
                if (!(norm > 0.0) || !std::isfinite(norm)) return false;
                dir[i] /= norm;
            }
            angle.assign(size_t(n) * n, 0.0);
            sep.assign(size_t(n) * n, 0.0);
            for (int32_t i = 0; i < n; ++i)
                for (int32_t j = i + 1; j < n; ++j)
                {
                    double a, s;
                    pairInvariant(mid[i], dir[i], mid[j], dir[j], a, s);
                    angle[size_t(i) * n + j] =
                        angle[size_t(j) * n + i] = a;
                    sep[size_t(i) * n + j] = sep[size_t(j) * n + i] = s;
                }
            return true;
        }

        void sortBySeparation(int32_t n, const std::vector<double>& sep,
                              std::vector<SepEntry>& out)
        {
            out.clear();
            if (n < 2) return;
            out.resize(size_t(n) * (n - 1));
            for (int32_t i = 0; i < n; ++i)
            {
                SepEntry* row = out.data() + size_t(i) * (n - 1);
                int32_t k = 0;
                for (int32_t j = 0; j < n; ++j)
                    if (j != i) row[k++] = {sep[size_t(i) * n + j], j};
                std::sort(row, row + k,
                          [](const SepEntry& a, const SepEntry& b) {
                              return a.sep != b.sep ? a.sep < b.sep
                                                    : a.j < b.j;
                          });
            }
        }
    }  // namespace detail

    template class RigidLines3D<LabelRules>;
    template bool buildNodesFirst(const RigidLines3D<LabelRules>&,
                                  const BuildParams&, Progress&,
                                  GraphRows&);
    template bool buildGraph(const RigidLines3D<LabelRules>&,
                             const BuildParams&, Progress&, GraphRows&);
}  // namespace AlignCalc
