#include "aligncalc.internal.h"

namespace AlignCalc
{
    static void invertCombi(int32_t ii, int32_t n, int32_t& x,
                            int32_t& y, int32_t& z)
    {
        x = 0;
        y = 0;
        z = 0;
        /* (x, y, z) is the ith element in the lexicographic ordering
         * of the elements in choose(n, 3). solve for x, y, z.
         * NOTE: 0 <= ii, x, y, z < n */

        /* choose(n, 2) elements will start with 0,
         * choose(n-x, 2) elements with start with x */
        for (x = 0; ii >= ((n - x - 1) * (n - x - 2)) / 2; ++x)
        {
            ii -= ((n - x - 1) * (n - x - 2)) / 2;
        }

        /* choose ((n-x)-2, 1) elements will start with x, x+1
         * choose ((n-x)-2-y,1) elements will start with x, x+y+1 */
        for (y = 0; ii >= ((n - x) - 2 - y); ++y)
        {
            ii -= ((n - x) - 2 - y);
        }

        y = (x + 1) + y;
        z = (y + 1) + ii;
    }

    void loadTaskInfo(int32_t ii1, int32_t N1, int32_t ii2, int32_t N2,
                      TaskInfo& task)
    {
        invertCombi(ii1, N1, task.i1, task.j1, task.k1);
        invertCombi(ii2, N2, task.i2, task.j2, task.k2);
        for (int a = 0; a < 8; ++a) task.check[a] = 0;
    }

    bool processTaskInfo(const DoubleMatrixR& left_pts,
                         const DoubleMatrixR& right_pts, TaskInfo& task,
                         double delta, double epsilon)
    {
        Triangle2D tl;
        Triangle2D tr;
        tl.construct(left_pts, task.i1, task.j1, task.k1);
        if (!tl.valid()) return false;
        tr.construct(right_pts, task.i2, task.j2, task.k2);
        if (!tr.valid()) return false;
        return tl.compare(tr, task, delta, epsilon);
    }

} /* namespace AlignCalc */
