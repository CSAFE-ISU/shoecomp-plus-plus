#ifndef SHOECOMP_CORE_ALIGNMENT_H
#define SHOECOMP_CORE_ALIGNMENT_H

#include "jtjson/json.h"

namespace shoecomp
{
    enum class AlignMode
    {
        Manual = 0,
        Automatic = 1
    };

    // A 2-D right->left similarity: rotation (radians), translation
    // and scale in image pixels, plus auxiliary data (matched points).
    struct AlignState
    {
        AlignMode mode = AlignMode::Manual;
        float rotation = 0.0f;
        float dx = 0.0f;
        float dy = 0.0f;
        float scale = 1.0f;
        jt::Json info;
    };
}  // namespace shoecomp

#endif
