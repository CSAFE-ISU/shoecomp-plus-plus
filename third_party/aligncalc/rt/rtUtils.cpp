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

            /* now 0 <= ii < n-x-1, so y = ii solves for (x, y) */
            y = ii;
        }

        void loadTaskInfo(int32_t ii1, int32_t N1, int32_t ii2,
                          int32_t N2, TaskInfo& task)
        {
            invertCombi(ii1, N1, task.i1, task.j1);
            invertCombi(ii2, N2, task.i2, task.j2);
            task.check = 0;
        }

        bool processTaskInfo(const DoubleMatrixR& left_pts,
                             const DoubleMatrixR& right_pts,
                             TaskInfo& task, double epsilon)
        {
            double d1 =
                std::hypot(left_pts(task.i1, 0) - left_pts(task.j1, 0),
                           left_pts(task.i1, 1) - left_pts(task.j1, 1));
            double d2 = std::hypot(
                right_pts(task.i2, 0) - right_pts(task.j2, 0),
                right_pts(task.i2, 1) - right_pts(task.j2, 1));
            if (std::abs(d1 - d2) < epsilon) { task.check = 1; }
            return task.check == 1;
        }

    } /* namespace RT */
} /* namespace AlignCalc */
