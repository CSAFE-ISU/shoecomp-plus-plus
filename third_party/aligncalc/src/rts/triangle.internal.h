#ifndef ALIGNCALC_TRIANGLE_INTERNAL_H
#define ALIGNCALC_TRIANGLE_INTERNAL_H
#include "aligncalc/alignTypes.h"
#include <cstdint>

namespace AlignCalc
{
    namespace RTS
    {
        struct Triangle
        {
            double as, bs, cs; /* sides */
            double at, bt, ct; /* angles */
            /* as is the side opposite to at */
            void construct(const DoubleMatrixR &, const int32_t,
                           const int32_t, const int32_t);
            bool valid() const;
            /* check[n] is set for each of the six vertex orderings
             * n under which |other| matches this triangle */
            bool compare(const Triangle &, uint8_t check[6], double,
                         double) const;
        };
    } /* namespace RTS */

} /* namespace AlignCalc */

#endif
