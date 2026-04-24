#ifndef SHOECOMP_UI_SPLASH_H
#define SHOECOMP_UI_SPLASH_H

#include <string>
#include <vector>

namespace shoecomp
{
    struct SplashResult
    {
        bool cancelled = false;
        std::vector<std::string> licenseTexts;
    };

    SplashResult runSplash(double duration);
}  // namespace shoecomp

#endif
