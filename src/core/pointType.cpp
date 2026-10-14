#include "core/pointType.h"

namespace shoecomp
{
    const char* pointTypeToString(PointType t)
    {
        switch (t)
        {
            case PointType::Center:
                return "Center";
            case PointType::RidgeEnding:
                return "RidgeEnding";
            case PointType::Bifurcation:
                return "Bifurcation";
            case PointType::Other:
                return "Other";
            case PointType::Core:
                return "Core";
            case PointType::Delta:
                return "Delta";
            case PointType::WordStart:
                return "WordStart";
            case PointType::WordEnd:
                return "WordEnd";
            case PointType::Intersection:
                return "Intersection";
            case PointType::CurveTurn:
                return "CurveTurn";
            case PointType::Peak:
                return "Peak";
            case PointType::Corner:
            default:
                return "Corner";
        }
    }

    PointType stringToPointType(const std::string& s)
    {
        if (s == "Center") return PointType::Center;
        if (s == "RidgeEnding") return PointType::RidgeEnding;
        if (s == "Bifurcation") return PointType::Bifurcation;
        if (s == "Other") return PointType::Other;
        if (s == "Core") return PointType::Core;
        if (s == "Delta") return PointType::Delta;
        if (s == "WordStart") return PointType::WordStart;
        if (s == "WordEnd") return PointType::WordEnd;
        if (s == "Intersection") return PointType::Intersection;
        if (s == "CurveTurn") return PointType::CurveTurn;
        if (s == "Peak") return PointType::Peak;
        return PointType::Corner;
    }
}  // namespace shoecomp
