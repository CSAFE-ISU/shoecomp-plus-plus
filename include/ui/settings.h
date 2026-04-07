#ifndef SHOECOMP_UI_SETTINGS_H
#define SHOECOMP_UI_SETTINGS_H

namespace shoecomp
{
    struct SettingsState
    {
        // App
        int themeIdx = 3;  // Material Flat
        bool fullscreen = true;
        float fontScale = 2.5f;

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
    };

    void renderSettingsTab(SettingsState& s);
    void applyTheme(int themeIdx);
}  // namespace shoecomp

#endif
