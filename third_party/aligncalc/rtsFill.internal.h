#ifndef ALIGNCALC_RTSFILL_INTERNAL_H
#define ALIGNCALC_RTSFILL_INTERNAL_H
#include "aligncalc.h"
#include "graphInfo.internal.h"

namespace AlignCalc
{
    namespace RTS
    {
        struct TaskInfo
        {
            int32_t i1, j1, k1;
            int32_t i2, j2, k2;
            uint8_t check[8];
        };

        struct Triangle2D
        {
            double as, bs, cs; /* sides */
            double at, bt, ct; /* angles */
            /* as is the side opposite to at */
            void construct(const DoubleMatrixR &, const int32_t,
                           const int32_t, const int32_t);
            bool valid() const;
            bool compare(const Triangle2D &, TaskInfo &, double,
                         double) const;
        };

        class Updater
        {
            Node v1, v2, v3;
            void update0(const TaskInfo &);
            void update1(const TaskInfo &);
            void update2(const TaskInfo &);
            void update3(const TaskInfo &);
            void update4(const TaskInfo &);
            void update5(const TaskInfo &);

           public:
            void updateAll(GraphInfo &, const TaskInfo &);
        };

        void loadTaskInfo(int32_t, int32_t, int32_t, int32_t,
                          TaskInfo &);
        bool processTaskInfo(const DoubleMatrixR &,
                             const DoubleMatrixR &, TaskInfo &, double,
                             double);
        bool fillGraph(const DoubleMatrixR &, const DoubleMatrixR &,
                       const RTSParams &, WorkerChannel &, GraphInfo &);
    } /* namespace RTS */

} /* namespace AlignCalc */

#endif
