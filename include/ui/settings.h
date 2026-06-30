#ifndef SHOECOMP_UI_SETTINGS_H
#define SHOECOMP_UI_SETTINGS_H

#include "imgui.h"
#include "ui/imageCanvas.h"
#include <string>

namespace shoecomp
{
    struct SettingsState
    {
        // App
        int themeIdx = 6;  // ShoeComp Dark
        float fontScale = 2.5f;

        // Active canvas kind: which ImageCanvas subclass new images
        // load as. Only ShoeCanvas / EBTSCanvas are user-selectable.
        ImageCanvas::Kind activeKind = ImageCanvas::Kind::ShoeCanvas;

        // Locked-viewer hover indicators
        // Shared radius for source (cyan) and transformed (orange)
        // cursors.
        float cursorRadius = 8.0f;
        ImVec4 cursorColor =
            ImVec4(0.0f, 220 / 255.f, 1.0f, 200 / 255.f);
        ImVec4 transformedColor =
            ImVec4(1.0f, 150 / 255.f, 0.0f, 180 / 255.f);

        // Green matched-point circle
        float correspondingRadius = 8.0f;
        ImVec4 correspondingColor =
            ImVec4(0.0f, 1.0f, 100 / 255.f, 180 / 255.f);

        float hoverThreshold = 15.0f;  // pixels

        // Gallery: highlight color for the active image window
        ImVec4 activeImageColor = ImVec4(0.75f, 0.49f, 0.0f, 1.0f);
    };

    void renderSettingsTab(SettingsState& s, ImageCanvas& activeCanvas);
    void applyTheme(int themeIdx);
    void applyModernStyling();
    void registerSettingsHandler(SettingsState& s);
}  // namespace shoecomp

#endif
