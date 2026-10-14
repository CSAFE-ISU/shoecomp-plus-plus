#include "core/pointType.h"

namespace shoecomp
{
    PointColor defaultPointColor(PointType t)
    {
        switch (t)
        {
            case PointType::Center:
                return {0.2f, 0.39f, 1.0f, 0.86f};
            case PointType::RidgeEnding:
                return {1.0f, 0.85f, 0.0f, 0.86f};
            case PointType::Bifurcation:
                return {1.0f, 0.55f, 0.0f, 0.86f};
            case PointType::Other:
                return {0.8f, 0.8f, 0.0f, 0.86f};
            case PointType::Core:
                return {0.0f, 0.85f, 0.85f, 0.86f};
            case PointType::Delta:
                return {0.85f, 0.0f, 0.85f, 0.86f};
            case PointType::WordStart:
                return {0.1f, 0.8f, 0.35f, 0.86f};
            case PointType::WordEnd:
                return {0.95f, 0.22f, 0.12f, 0.86f};
            case PointType::Intersection:
                return {0.65f, 0.1f, 0.9f, 0.86f};
            case PointType::CurveTurn:
                return {0.95f, 0.62f, 0.05f, 0.86f};
            case PointType::Peak:
                return {1.0f, 0.85f, 0.1f, 0.95f};
            case PointType::Corner:
            default:
                return {1.0f, 0.2f, 0.2f, 0.86f};
        }
    }

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
