#ifndef ALIGNCALC_RTFILL_INTERNAL_H
#define ALIGNCALC_RTFILL_INTERNAL_H
#include "aligncalc.h"
#include "clq/graphInfo.internal.h"

namespace AlignCalc
{
    namespace RT
    {
        struct TaskInfo
        {
            int32_t i1, j1;
            int32_t i2, j2;
            uint8_t check;
        };

        class Updater
        {
            Node v1, v2;
            void update0(const TaskInfo &);
            void update1(const TaskInfo &);

           public:
            void updateAll(GraphInfo &, const TaskInfo &);
        };

        void loadTaskInfo(int32_t, int32_t, int32_t, int32_t,
                          TaskInfo &);
        bool processTaskInfo(const DoubleMatrixR &,
                             const DoubleMatrixR &, TaskInfo &, double);
        bool fillGraph(const DoubleMatrixR &, const DoubleMatrixR &,
                       const RTSParams &, WorkerChannel &, GraphInfo &);
    } /* namespace RT */

} /* namespace AlignCalc */

#endif
