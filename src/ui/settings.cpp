#include "ui/settings.h"
#include "ui/uiHelpers.h"
#include "ui/imageCanvas.h"
#include "hello_imgui/hello_imgui.h"
#include "hello_imgui/imgui_theme.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>

namespace shoecomp
{
    // Theme mapping
    static const ImGuiTheme::ImGuiTheme_ themeMap[] = {
        ImGuiTheme::ImGuiTheme_ImGuiColorsClassic,
        ImGuiTheme::ImGuiTheme_ImGuiColorsDark,
        ImGuiTheme::ImGuiTheme_ImGuiColorsLight,
        ImGuiTheme::ImGuiTheme_MaterialFlat,
        ImGuiTheme::ImGuiTheme_PhotoshopStyle,
        ImGuiTheme::ImGuiTheme_GrayVariations,
        ImGuiTheme::ImGuiTheme_GrayVariations_Darker,
        ImGuiTheme::ImGuiTheme_MicrosoftStyle,
        ImGuiTheme::ImGuiTheme_Cherry,
        ImGuiTheme::ImGuiTheme_Darcula,
        ImGuiTheme::ImGuiTheme_DarculaDarker,
        ImGuiTheme::ImGuiTheme_LightRounded,
        ImGuiTheme::ImGuiTheme_SoDark_AccentBlue,
        ImGuiTheme::ImGuiTheme_SoDark_AccentYellow,
        ImGuiTheme::ImGuiTheme_SoDark_AccentRed,
        ImGuiTheme::ImGuiTheme_BlackIsBlack,
        ImGuiTheme::ImGuiTheme_WhiteIsWhite,
    };

    void applyModernStyling()
    {
        ImGuiStyle& style = ImGui::GetStyle();

        // Rounding - modern apps use subtle, consistent rounding
        style.WindowRounding = 8.0f;
        style.ChildRounding = 6.0f;
        style.FrameRounding = 4.0f;
        style.PopupRounding = 6.0f;
        style.ScrollbarRounding = 9.0f;
        style.GrabRounding = 4.0f;
        style.TabRounding = 4.0f;

        // Borders - subtle, minimal borders
        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.TabBorderSize = 0.0f;

        // Padding - comfortable spacing
        style.WindowPadding = ImVec2(12.0f, 12.0f);
        style.FramePadding = ImVec2(8.0f, 4.0f);
        style.CellPadding = ImVec2(6.0f, 4.0f);

        // Spacing - consistent gaps
        style.ItemSpacing = ImVec2(8.0f, 6.0f);
        style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
        style.IndentSpacing = 20.0f;

        // Interactive elements - comfortable sizes
        style.ScrollbarSize = 14.0f;
        style.GrabMinSize = 12.0f;

        // Alignment - centered titles, left-aligned buttons
        style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
        style.ButtonTextAlign = ImVec2(0.5f, 0.5f);

        // Window constraints
        style.WindowMinSize = ImVec2(200.0f, 100.0f);

        // Display safe areas
        style.DisplaySafeAreaPadding = ImVec2(4.0f, 4.0f);
    }

    void applyTheme(int themeIdx)
    {
        int idx = std::clamp(themeIdx, 0, 16);
        ImGuiTheme::ApplyTheme(themeMap[idx]);
        applyModernStyling();
    }

    void renderSettingsTab(SettingsState& s)
    {
        // Update Settings button at the top
        if (ImGui::Button("Update Settings", ImVec2(-1.0f, 0.0f)))
        {
            applyTheme(s.themeIdx);
            HelloImGui::GetRunnerParams()
                ->appWindowParams.windowGeometry.fullScreenMode =
                s.fullscreen
                    ? HelloImGui::FullScreenMode::FullMonitorWorkArea
                    : HelloImGui::FullScreenMode::NoFullScreen;
            ImGui::GetIO().FontGlobalScale = s.fontScale;
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // General Settings section
        if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen))
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
            const char* themeNames =
                "ImGui Classic\0"
                "ImGui Dark\0"
                "ImGui Light\0"
                "Material Flat\0"
                "Photoshop Style\0"
                "Gray Variations\0"
                "Gray Variations Darker\0"
                "Microsoft Style\0"
                "Cherry\0"
                "Darcula\0"
                "Darcula Darker\0"
                "Light Rounded\0"
                "SoDark Accent Blue\0"
                "SoDark Accent Yellow\0"
                "SoDark Accent Red\0"
                "Black Is Black\0"
                "White Is White\0";
            ImGui::Combo("##Theme", &s.themeIdx, themeNames);

            settingsTableRow("Fullscreen");
            ImGui::Checkbox("##Fullscreen", &s.fullscreen);

            settingsTableRow("Font Scale");
            ImGui::SliderFloat("##FontScale", &s.fontScale, 0.5f, 4.0f,
                               "%.2f");
            s.fontScale = std::round(s.fontScale / 0.05f) * 0.05f;

            ImGui::EndTable();
            }
        }

        ImGui::Spacing();

        // Annotations section
        if (ImGui::CollapsingHeader("Annotations", ImGuiTreeNodeFlags_DefaultOpen))
        {
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
        }

        ImGui::Spacing();

        // Locked Viewer Indicators section
        if (ImGui::CollapsingHeader("Locked Viewer Indicators", ImGuiTreeNodeFlags_DefaultOpen))
        {
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
        }
    }
}  // namespace shoecomp
