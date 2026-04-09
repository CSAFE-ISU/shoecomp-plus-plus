#ifndef SHOECOMP_UI_SPLASH_H
#define SHOECOMP_UI_SPLASH_H

namespace shoecomp
{
    struct SplashResult
    {
        bool cancelled = false;
    };

    SplashResult runSplash(double duration, int progressSteps = 5);
}  // namespace shoecomp

#endif
