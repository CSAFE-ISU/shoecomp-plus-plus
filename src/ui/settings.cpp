#include "ui/settings.h"
#include "ui/uiHelpers.h"
#include "ui/imageCanvas.h"
#include "hello_imgui/hello_imgui.h"
#include "hello_imgui/imgui_theme.h"
#include "imgui.h"
#include <cmath>

namespace shoecomp
{
    void renderSettingsTab(SettingsState& s)
    {
        if (ImGui::BeginTable("##settings", 3))
        {
            ImGui::TableSetupColumn(
                "Label", ImGuiTableColumnFlags_WidthFixed, 250.0f);
            ImGui::TableSetupColumn(
                "Spacer", ImGuiTableColumnFlags_WidthFixed, 20.0f);
            ImGui::TableSetupColumn("Widget",
                                    ImGuiTableColumnFlags_WidthStretch);

            settingsTableRow("Theme");
            ImGui::Combo("##Theme", &s.themeIdx, "Light\0Dark\0");

            settingsTableRow("Fullscreen");
            ImGui::Checkbox("##Fullscreen", &s.fullscreen);

            settingsTableRow("Font Scale");
            ImGui::SliderFloat("##FontScale", &s.fontScale, 0.5f, 4.0f,
                               "%.2f");
            s.fontScale = std::round(s.fontScale / 0.05f) * 0.05f;

            ImGui::EndTable();
        }
        ImGui::Spacing();
        ImGui::SeparatorText("Annotations");
        if (ImGui::BeginTable("##annSettings", 3))
        {
            ImGui::TableSetupColumn(
                "Label", ImGuiTableColumnFlags_WidthFixed, 250.0f);
            ImGui::TableSetupColumn(
                "Spacer", ImGuiTableColumnFlags_WidthFixed, 20.0f);
            ImGui::TableSetupColumn("Widget",
                                    ImGuiTableColumnFlags_WidthStretch);

            settingsTableRow("Point Radius");
            ImGui::SliderFloat("##PointRadius",
                               &g_annotationStyle.pointRadius, 2.0f,
                               15.0f, "%.1f");

            settingsTableRow("Corner Color");
            ImGui::ColorEdit4("##CornerColor",
                              g_annotationStyle.cornerColor);

            settingsTableRow("Center Color");
            ImGui::ColorEdit4("##CenterColor",
                              g_annotationStyle.centerColor);

            settingsTableRow("Bounds Thickness");
            ImGui::SliderFloat("##BoundsThickness",
                               &g_annotationStyle.boundsLineThickness,
                               1.0f, 8.0f, "%.1f");

            settingsTableRow("Bounds Color");
            ImGui::ColorEdit4("##BoundsColor",
                              g_annotationStyle.boundsColor);

            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::SeparatorText("Locked Viewer Indicators");
        if (ImGui::BeginTable("##lockedSettings", 3))
        {
            ImGui::TableSetupColumn(
                "Label", ImGuiTableColumnFlags_WidthFixed, 250.0f);
            ImGui::TableSetupColumn(
                "Spacer", ImGuiTableColumnFlags_WidthFixed, 20.0f);
            ImGui::TableSetupColumn("Widget",
                                    ImGuiTableColumnFlags_WidthStretch);

            settingsTableRow("Cursor Radius");
            ImGui::SliderFloat("##CursorRadius", &s.cursorRadius, 2.0f,
                               20.0f, "%.1f");

            settingsTableRow("Cursor Color");
            ImGui::ColorEdit4("##CursorColor", s.cursorColor);

            settingsTableRow("Transformed Cursor Color");
            ImGui::ColorEdit4("##TransformedColor", s.transformedColor);

            settingsTableRow("Corresponding Radius");
            ImGui::SliderFloat("##CorrespondingRadius",
                               &s.correspondingRadius, 2.0f, 20.0f,
                               "%.1f");

            settingsTableRow("Corresponding Color");
            ImGui::ColorEdit4("##CorrespondingColor",
                              s.correspondingColor);

            settingsTableRow("Hover Threshold (px)");
            ImGui::SliderFloat("##HoverThreshold", &s.hoverThreshold,
                               1.0f, 100.0f, "%.1f");

            ImGui::EndTable();
        }

        ImGui::Spacing();
        if (ImGui::Button("Update Settings"))
        {
            auto theme = s.themeIdx == 0
                             ? ImGuiTheme::ImGuiTheme_ImGuiColorsLight
                             : ImGuiTheme::ImGuiTheme_ImGuiColorsDark;
            ImGuiTheme::ApplyTheme(theme);
            HelloImGui::GetRunnerParams()
                ->appWindowParams.windowGeometry.fullScreenMode =
                s.fullscreen
                    ? HelloImGui::FullScreenMode::FullMonitorWorkArea
                    : HelloImGui::FullScreenMode::NoFullScreen;
            ImGui::GetIO().FontGlobalScale = s.fontScale;
        }
    }
}  // namespace shoecomp
