#ifndef SHOECOMP_IMAGE_CALCHELPERS_H
#define SHOECOMP_IMAGE_CALCHELPERS_H

#include "imgui.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

namespace shoecomp
{
    inline float length(const ImVec2& v0)
    {
        return sqrtf(v0.x * v0.x + v0.y * v0.y);
    }

    inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs)
    {
        return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y);
    }

    inline ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs)
    {
        return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y);
    }

    inline ImVec2 operator+=(ImVec2& lhs, const ImVec2& rhs)
    {
        lhs.x += rhs.x;
        lhs.y += rhs.y;
        return lhs;
    }

    inline ImVec2 operator*(float s, const ImVec2& rhs)
    {
        return ImVec2(s * rhs.x, s * rhs.y);
    }

    inline ImVec2 operator*(const ImVec2& lhs, float s)
    {
        return ImVec2(lhs.x * s, lhs.y * s);
    }

    inline ImVec2 operator/(const ImVec2& lhs, float s)
    {
        return ImVec2(lhs.x / s, lhs.y / s);
    }

    inline ImVec2 min(const ImVec2& lhs, const ImVec2& rhs)
    {
        return ImVec2(std::min(lhs.x, rhs.x), std::min(lhs.y, rhs.y));
    }

    inline ImVec2 max(const ImVec2& lhs, const ImVec2& rhs)
    {
        return ImVec2(std::max(lhs.x, rhs.x), std::max(lhs.y, rhs.y));
    }

    inline ImVec2 clamp(const ImVec2& lhs, const ImVec2& rhs)
    {
        return ImVec2(
            std::clamp(lhs.x, -std::abs(rhs.x), std::abs(rhs.x)),
            std::clamp(lhs.y, -std::abs(rhs.y), std::abs(rhs.y)));
    }

    inline ImVec2 direction(float theta)
    {
        return ImVec2(cosf(theta), sinf(theta));
    }

}  // namespace shoecomp

#endif
