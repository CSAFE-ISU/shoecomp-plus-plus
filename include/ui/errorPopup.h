#ifndef SHOECOMP_ERROR_POPUP
#define SHOECOMP_ERROR_POPUP

#include <string>

namespace shoecomp
{
    struct ErrorPopup
    {
        bool show = false;
        std::string message;
        std::string title;
        void render();
    };
}  // namespace shoecomp

#endif
