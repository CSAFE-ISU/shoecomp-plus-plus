#include "ui/settings.h"
#include "ui/uiHelpers.h"
#include "ui/imageCanvas.h"
#include "hello_imgui/hello_imgui.h"
#include "hello_imgui/imgui_theme.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace shoecomp
{
    static const char* themeNames =
        "ImGui Dark\0"
        "ImGui Light\0"
        "Material Flat\0"
        "Photoshop Style\0"
        "Microsoft Style\0"
        "ShoeComp Light\0"
        "ShoeComp Dark\0";

    // Theme mapping
    static const ImGuiTheme::ImGuiTheme_ themeMap[] = {
        ImGuiTheme::ImGuiTheme_ImGuiColorsDark,
        ImGuiTheme::ImGuiTheme_ImGuiColorsLight,
        ImGuiTheme::ImGuiTheme_MaterialFlat,
        ImGuiTheme::ImGuiTheme_PhotoshopStyle,
        ImGuiTheme::ImGuiTheme_MicrosoftStyle,
    };

    static ImVec4 hex(uint32_t rgb, float a = 1.0f)
    {
        float r = ((rgb >> 16) & 0xFF) / 255.0f;
        float g = ((rgb >> 8) & 0xFF) / 255.0f;
        float b = (rgb & 0xFF) / 255.0f;
        return ImVec4(r, g, b, a);
    }

    // ShoeComp palette accents (shared across light/dark)
    // Primary: 1E7BC0, hover 40B4E5, active 003A70
    // Success 00833E, Error CF0A2C, Warning FA8D29
    static void applyShoeCompAccents(bool dark)
    {
        ImVec4* c = ImGui::GetStyle().Colors;
        ImVec4 accent = hex(0x1E7BC0);
        ImVec4 accentHover = hex(0x40B4E5);
        ImVec4 accentActive = hex(0x003A70);
        ImVec4 sep = dark ? hex(0x8A8A8D) : hex(0xBDBCBC);

        c[ImGuiCol_CheckMark] = accent;
        c[ImGuiCol_SliderGrab] = accent;
        c[ImGuiCol_SliderGrabActive] = accentHover;
        c[ImGuiCol_Button] = accent;
        c[ImGuiCol_ButtonHovered] = accentHover;
        c[ImGuiCol_ButtonActive] = accentActive;
        c[ImGuiCol_Header] =
            ImVec4(accent.x, accent.y, accent.z, 0.55f);
        c[ImGuiCol_HeaderHovered] = accentHover;
        c[ImGuiCol_HeaderActive] = accentActive;
        c[ImGuiCol_Separator] = sep;
        c[ImGuiCol_SeparatorHovered] = accentHover;
        c[ImGuiCol_SeparatorActive] = accent;
        c[ImGuiCol_Border] = sep;
        c[ImGuiCol_Tab] = ImVec4(accent.x, accent.y, accent.z, 0.55f);
        c[ImGuiCol_TabHovered] = accentHover;
        c[ImGuiCol_TabActive] = accent;
        c[ImGuiCol_TabUnfocused] =
            ImVec4(accent.x, accent.y, accent.z, 0.35f);
        c[ImGuiCol_TabUnfocusedActive] =
            ImVec4(accent.x, accent.y, accent.z, 0.75f);
        c[ImGuiCol_DockingPreview] =
            ImVec4(accent.x, accent.y, accent.z, 0.7f);
        c[ImGuiCol_PlotLines] = accent;
        c[ImGuiCol_PlotLinesHovered] = hex(0xFA8D29);
        c[ImGuiCol_PlotHistogram] = hex(0x00833E);
        c[ImGuiCol_PlotHistogramHovered] = hex(0xCF0A2C);
        c[ImGuiCol_TextSelectedBg] =
            ImVec4(accent.x, accent.y, accent.z, 0.45f);
        c[ImGuiCol_NavHighlight] = accentHover;
        c[ImGuiCol_TitleBgActive] = accentActive;
    }

    static void applyShoeCompLight()
    {
        ImGui::StyleColorsLight();
        ImVec4* c = ImGui::GetStyle().Colors;
        c[ImGuiCol_Text] = hex(0x1A1A1A);
        c[ImGuiCol_WindowBg] = hex(0xBDBCBC);
        c[ImGuiCol_ChildBg] = hex(0xBDBCBC);
        c[ImGuiCol_PopupBg] = hex(0xBDBCBC);
        c[ImGuiCol_FrameBg] = hex(0xA3A2A2);
        c[ImGuiCol_FrameBgHovered] = hex(0xE1ECF7);
        c[ImGuiCol_FrameBgActive] = hex(0xD0E3F5);
        c[ImGuiCol_TitleBg] = hex(0xBDBCBC);
        applyShoeCompAccents(false);
    }

    static void applyShoeCompDark()
    {
        // Base on MaterialFlat (keeps its background palette),
        // then overlay the ShoeComp accent chain on top.
        applyShoeCompAccents(true);
    }

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
        int idx = std::clamp(themeIdx, 0, 6);
        if (idx == 5)
        {
            ImGuiTheme::ApplyTheme(
                ImGuiTheme::ImGuiTheme_ImGuiColorsLight);
            applyShoeCompLight();
        }
        else if (idx == 6)
        {
            ImGuiTheme::ApplyTheme(ImGuiTheme::ImGuiTheme_MaterialFlat);
            applyShoeCompDark();
        }
        else
        {
            ImGuiTheme::ApplyTheme(themeMap[idx]);
        }
        applyModernStyling();
    }

    void renderSettingsTab(SettingsState& s)
    {
        // Update Settings button at the top
        if (ImGui::Button("Update Settings", ImVec2(-1.0f, 0.0f)))
        {
            applyTheme(s.themeIdx);
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
                                  &g_annotationStyle.cornerColor.x);

                settingsTableRow("Center Color");
                ImGui::ColorEdit4("##CenterColor",
                                  &g_annotationStyle.centerColor.x);

                settingsTableRow("Ridge Ending Color");
                ImGui::ColorEdit4(
                    "##RidgeEndingColor",
                    &g_annotationStyle.ridgeEndingColor.x);

                settingsTableRow("Bifurcation Color");
                ImGui::ColorEdit4(
                    "##BifurcationColor",
                    &g_annotationStyle.bifurcationColor.x);

                settingsTableRow("Other Color");
                ImGui::ColorEdit4("##OtherColor",
                                  &g_annotationStyle.otherColor.x);

                settingsTableRow("Core Color");
                ImGui::ColorEdit4("##CoreColor",
                                  &g_annotationStyle.coreColor.x);

                settingsTableRow("Delta Color");
                ImGui::ColorEdit4("##DeltaColor",
                                  &g_annotationStyle.deltaColor.x);

                settingsTableRow("Bounds Thickness");
                ImGui::SliderFloat(
                    "##BoundsThickness",
                    &g_annotationStyle.boundsLineThickness, 1.0f, 8.0f,
                    "%.1f");

                settingsTableRow("Bounds Color");
                ImGui::ColorEdit4("##BoundsColor",
                                  &g_annotationStyle.boundsColor.x);

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
                ImGui::ColorEdit4("##CursorColor", &s.cursorColor.x);

                settingsTableRow("Transformed Cursor Color");
                ImGui::ColorEdit4("##TransformedColor",
                                  &s.transformedColor.x);

                settingsTableRow("Corresponding Radius");
                ImGui::SliderFloat("##CorrespondingRadius",
                                   &s.correspondingRadius, 2.0f, 20.0f,
                                   "%.1f");

                settingsTableRow("Corresponding Color");
                ImGui::ColorEdit4("##CorrespondingColor",
                                  &s.correspondingColor.x);

                settingsTableRow("Hover Threshold (px)");
                ImGui::SliderFloat("##HoverThreshold",
                                   &s.hoverThreshold, 1.0f, 100.0f,
                                   "%.1f");

                settingsTableRow("Active Image Color");
                ImGui::ColorEdit4("##ActiveImageColor",
                                  &s.activeImageColor.x);

                ImGui::EndTable();
            }
        }
    }

    static void* settingsReadOpen(ImGuiContext*,
                                  ImGuiSettingsHandler* handler,
                                  const char*)
    {
        return handler->UserData;
    }

    static void settingsReadLine(ImGuiContext*,
                                 ImGuiSettingsHandler* handler, void*,
                                 const char* line)
    {
        auto* s = (SettingsState*)handler->UserData;
        if (!s) return;
        int i;
        float f;
        if (sscanf(line, "themeIdx=%d", &i) == 1)
            s->themeIdx = i;
        else if (sscanf(line, "fontScale=%f", &f) == 1)
            s->fontScale = f;
        else if (sscanf(line, "cursorRadius=%f", &f) == 1)
            s->cursorRadius = f;
        else if (sscanf(line, "correspondingRadius=%f", &f) == 1)
            s->correspondingRadius = f;
        else if (sscanf(line, "hoverThreshold=%f", &f) == 1)
            s->hoverThreshold = f;
        else
        {
            float a, b, c, d;
            if (sscanf(line, "activeImageColor=%f,%f,%f,%f", &a, &b, &c,
                       &d) == 4)
            {
                s->activeImageColor.x = a;
                s->activeImageColor.y = b;
                s->activeImageColor.z = c;
                s->activeImageColor.w = d;
            }
        }
    }

    static void settingsWriteAll(ImGuiContext*,
                                 ImGuiSettingsHandler* handler,
                                 ImGuiTextBuffer* buf)
    {
        auto* s = (SettingsState*)handler->UserData;
        if (!s) return;
        buf->appendf("[%s][State]\n", handler->TypeName);
        buf->appendf("themeIdx=%d\n", s->themeIdx);
        buf->appendf("fontScale=%.4f\n", s->fontScale);
        buf->appendf("cursorRadius=%.4f\n", s->cursorRadius);
        buf->appendf("correspondingRadius=%.4f\n",
                     s->correspondingRadius);
        buf->appendf("hoverThreshold=%.4f\n", s->hoverThreshold);
        buf->appendf("activeImageColor=%.4f,%.4f,%.4f,%.4f\n",
                     s->activeImageColor.x, s->activeImageColor.y,
                     s->activeImageColor.z, s->activeImageColor.w);
        buf->append("\n");
    }

    void registerSettingsHandler(SettingsState& s)
    {
        ImGuiContext* ctx = ImGui::GetCurrentContext();
        if (!ctx) return;
        for (auto& h : ctx->SettingsHandlers)
            if (strcmp(h.TypeName, "ShoeComp") == 0) return;

        ImGuiSettingsHandler handler;
        handler.TypeName = "ShoeComp";
        handler.TypeHash = ImHashStr("ShoeComp");
        handler.ReadOpenFn = settingsReadOpen;
        handler.ReadLineFn = settingsReadLine;
        handler.WriteAllFn = settingsWriteAll;
        handler.UserData = &s;
        ctx->SettingsHandlers.push_back(handler);
    }
}  // namespace shoecomp
