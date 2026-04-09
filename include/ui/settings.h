#ifndef SHOECOMP_UI_SETTINGS_H
#define SHOECOMP_UI_SETTINGS_H

#include <string>

namespace shoecomp
{
    struct SettingsState
    {
        // App
        int themeIdx = 2;  // Material Flat
        float fontScale = 2.5f;
        int windowWidth = 0;   // 0 = uninitialized (use default)
        int windowHeight = 0;

        // Locked-viewer hover indicators
        // Shared radius for source (cyan) and transformed (orange)
        // cursors.
        float cursorRadius = 8.0f;
        float cursorColor[4] = {0.0f, 220 / 255.f, 1.0f, 200 / 255.f};
        float transformedColor[4] = {1.0f, 150 / 255.f, 0.0f,
                                     180 / 255.f};

        // Green matched-point circle
        float correspondingRadius = 8.0f;
        float correspondingColor[4] = {0.0f, 1.0f, 100 / 255.f,
                                       180 / 255.f};

        float hoverThreshold = 15.0f;  // pixels

        // Gallery: highlight color for the active image window
        float activeImageColor[4] = {1.0f, 0.65f, 0.0f, 1.0f};
    };

    void renderSettingsTab(SettingsState& s);
    void applyTheme(int themeIdx);
    void applyModernStyling();
    void registerSettingsHandler(SettingsState& s);
}  // namespace shoecomp

#endif
