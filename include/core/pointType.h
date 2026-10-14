#ifndef SHOECOMP_CORE_POINT_TYPE_H
#define SHOECOMP_CORE_POINT_TYPE_H

#include <cstdint>
#include <string>

namespace shoecomp
{
    // Annotation point label. The numeric values are used as the
    // match label in 2-D auto-align, so keep them stable.
    enum class PointType : uint8_t
    {
        Corner = 0,
        Center = 1,
        RidgeEnding = 2,
        Bifurcation = 3,
        Other = 4,
        Core = 5,
        Delta = 6,
        WordStart = 7,
        WordEnd = 8,
        Intersection = 9,
        CurveTurn = 10,
        Peak = 11
    };

    const char* pointTypeToString(PointType t);
    // Unknown strings map to Corner.
    PointType stringToPointType(const std::string& s);
}  // namespace shoecomp

#endif
