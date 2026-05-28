#include "aligncalc.internal.h"

namespace AlignCalc
{
    namespace RT
    {

        static void invertCombi(int32_t ii, int32_t n, int32_t& x,
                                int32_t& y)
        {
            x = 0;
            y = 0;
            /* (x, y) is the ith element in the lexicographic
             * ordering of the elements in choose(n, 2). solve for x, y,
             * NOTE: 0 <= x, y < n; 0 <= ii < choose(n, 2) */

            /* choose(n-1, 1) elements will start with 0,
             * choose(n-x-1, 1) elements with start with x */
            for (x = 0; ii >= (n - x - 1); ++x) { ii -= (n - x - 1); }

            /* now 0 <= ii < n-x-1, so y = x + ii + 1 solves for (x, y)
             */
            y = x + ii + 1;
        }

        void loadTaskInfo(int32_t ii1, int32_t N1, int32_t ii2,
                          int32_t N2, TaskInfo& task)
        {
            invertCombi(ii1, N1, task.i1, task.j1);
            invertCombi(ii2, N2, task.i2, task.j2);
            task.check = 0;
        }

        static double l2dist(const DoubleMatrixR& pts, int32_t i,
                             int32_t j, int32_t colsize)
        {
            double d =
                (pts(Eigen::seq(i, i), Eigen::seq(0, colsize - 1)) -
                 pts(Eigen::seq(j, j), Eigen::seq(0, colsize - 1)))
                    .norm();
            return d;
        }

        bool processTaskInfo(const DoubleMatrixR& left_pts,
                             const DoubleMatrixR& right_pts,
                             TaskInfo& task, const RTSParams& params)
        {
            auto colsize = left_pts.cols();
            double d1 = l2dist(left_pts, task.i1, task.j1, colsize);
            double d2 = l2dist(right_pts, task.i2, task.j2, colsize);

            double low = d2 * (1 - params.epsilon);
            double hi = d2 * (1 + params.epsilon);
            if (d1 >= low && d1 <= hi) { task.check = 1; }
            return task.check == 1;
        }

    } /* namespace RT */
} /* namespace AlignCalc */
