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
    static const char* themeNames =
        "ImGui Dark\0"
        "ImGui Light\0"
        "Material Flat\0"
        "Photoshop Style\0"
        "Microsoft Style\0";

    // Theme mapping
    static const ImGuiTheme::ImGuiTheme_ themeMap[] = {
        ImGuiTheme::ImGuiTheme_ImGuiColorsDark,
        ImGuiTheme::ImGuiTheme_ImGuiColorsLight,
        ImGuiTheme::ImGuiTheme_MaterialFlat,
        ImGuiTheme::ImGuiTheme_PhotoshopStyle,
        ImGuiTheme::ImGuiTheme_MicrosoftStyle,
    };

    void applyModernStyling()
    {
        ImGuiStyle& style = ImGui::GetStyle();
        float fontSize = ImGui::GetFontSize();

        // Rounding - modern apps use subtle, consistent rounding
        style.WindowRounding = fontSize * kWindowRounding;
        style.ChildRounding = fontSize * kChildRounding;
        style.FrameRounding = fontSize * kFrameRounding;
        style.PopupRounding = fontSize * kPopupRounding;
        style.ScrollbarRounding = fontSize * kScrollbarRounding;
        style.GrabRounding = fontSize * kGrabRounding;
        style.TabRounding = fontSize * kTabRounding;

        // Borders - subtle, minimal borders
        style.WindowBorderSize = fontSize * kBorderSize;
        style.ChildBorderSize = fontSize * kBorderSize;
        style.PopupBorderSize = fontSize * kBorderSize;
        style.FrameBorderSize = 0.0f;
        style.TabBorderSize = 0.0f;

        // Padding - comfortable spacing
        style.WindowPadding = ImVec2(fontSize * kWindowPadding,
                                     fontSize * kWindowPadding);
        style.FramePadding = ImVec2(fontSize * kFramePaddingX,
                                    fontSize * kFramePaddingY);
        style.CellPadding =
            ImVec2(fontSize * kCellPaddingX, fontSize * kCellPaddingY);

        // Spacing - consistent gaps
        style.ItemSpacing =
            ImVec2(fontSize * kItemSpacingX, fontSize * kItemSpacingY);
        style.ItemInnerSpacing = ImVec2(fontSize * kItemInnerSpacingX,
                                        fontSize * kItemInnerSpacingY);
        style.IndentSpacing = fontSize * kIndentSpacing;

        // Interactive elements - comfortable sizes
        style.ScrollbarSize = fontSize * kScrollbarSize;
        style.GrabMinSize = fontSize * kGrabMinSize;

        // Alignment - centered titles, left-aligned buttons
        style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
        style.ButtonTextAlign = ImVec2(0.5f, 0.5f);

        // Window constraints
        style.WindowMinSize = ImVec2(fontSize * kWindowMinWidth,
                                     fontSize * kWindowMinHeight);

        // Display safe areas
        style.DisplaySafeAreaPadding = ImVec2(
            fontSize * kSafeAreaPadding, fontSize * kSafeAreaPadding);
    }

    void applyTheme(int themeIdx)
    {
        int idx = std::clamp(themeIdx, 0, 4);
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
        if (ImGui::CollapsingHeader("General",
                                    ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::BeginTable("##settings", 3))
            {
                ImGui::TableSetupColumn(
                    "Label", ImGuiTableColumnFlags_WidthFixed, 250.0f);
                ImGui::TableSetupColumn(
                    "Spacer", ImGuiTableColumnFlags_WidthFixed, 20.0f);
                ImGui::TableSetupColumn(
                    "Widget", ImGuiTableColumnFlags_WidthStretch);

                settingsTableRow("Theme");

                ImGui::Combo("##Theme", &s.themeIdx, themeNames);

                settingsTableRow("Fullscreen");
                ImGui::Checkbox("##Fullscreen", &s.fullscreen);

                settingsTableRow("Font Scale");
                ImGui::SliderFloat("##FontScale", &s.fontScale, 0.5f,
                                   4.0f, "%.2f");
                s.fontScale = std::round(s.fontScale / 0.05f) * 0.05f;

                ImGui::EndTable();
            }
        }

        ImGui::Spacing();

        // Annotations section
        if (ImGui::CollapsingHeader("Annotations",
                                    ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::BeginTable("##annSettings", 3))
            {
                ImGui::TableSetupColumn(
                    "Label", ImGuiTableColumnFlags_WidthFixed, 250.0f);
                ImGui::TableSetupColumn(
                    "Spacer", ImGuiTableColumnFlags_WidthFixed, 20.0f);
                ImGui::TableSetupColumn(
                    "Widget", ImGuiTableColumnFlags_WidthStretch);

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
                ImGui::SliderFloat(
                    "##BoundsThickness",
                    &g_annotationStyle.boundsLineThickness, 1.0f, 8.0f,
                    "%.1f");

                settingsTableRow("Bounds Color");
                ImGui::ColorEdit4("##BoundsColor",
                                  g_annotationStyle.boundsColor);

                ImGui::EndTable();
            }
        }

        ImGui::Spacing();

        // Locked Viewer Indicators section
        if (ImGui::CollapsingHeader("Locked Viewer Indicators",
                                    ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::BeginTable("##lockedSettings", 3))
            {
                ImGui::TableSetupColumn(
                    "Label", ImGuiTableColumnFlags_WidthFixed, 250.0f);
                ImGui::TableSetupColumn(
                    "Spacer", ImGuiTableColumnFlags_WidthFixed, 20.0f);
                ImGui::TableSetupColumn(
                    "Widget", ImGuiTableColumnFlags_WidthStretch);

                settingsTableRow("Cursor Radius");
                ImGui::SliderFloat("##CursorRadius", &s.cursorRadius,
                                   2.0f, 20.0f, "%.1f");

                settingsTableRow("Cursor Color");
                ImGui::ColorEdit4("##CursorColor", s.cursorColor);

                settingsTableRow("Transformed Cursor Color");
                ImGui::ColorEdit4("##TransformedColor",
                                  s.transformedColor);

                settingsTableRow("Corresponding Radius");
                ImGui::SliderFloat("##CorrespondingRadius",
                                   &s.correspondingRadius, 2.0f, 20.0f,
                                   "%.1f");

                settingsTableRow("Corresponding Color");
                ImGui::ColorEdit4("##CorrespondingColor",
                                  s.correspondingColor);

                settingsTableRow("Hover Threshold (px)");
                ImGui::SliderFloat("##HoverThreshold",
                                   &s.hoverThreshold, 1.0f, 100.0f,
                                   "%.1f");

                ImGui::EndTable();
            }
        }
    }
}  // namespace shoecomp
