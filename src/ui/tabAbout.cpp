#include "ui/mainWindow.h"
#include "ui/licenseData.h"
#include "imgui.h"

namespace shoecomp
{
    void renderAbout(AppState& state)
    {
        static int popupIdx = -1;

        const auto& licenses = getLicenses();

        if (state.boldFont) ImGui::PushFont(state.boldFont);
        ImGui::Text("ShoeComp");
        if (state.boldFont) ImGui::PopFont();

        ImGui::Spacing();
        ImGui::Text("Third-party licenses:");
        ImGui::Spacing();

        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGui::BeginChild("##licenseList", ImVec2(avail.x, avail.y),
                          ImGuiChildFlags_None,
                          ImGuiWindowFlags_AlwaysVerticalScrollbar);

        for (int i = 0; i < (int)licenses.size(); ++i)
        {
            const auto& lic = licenses[i];
            ImGui::PushID(i);

            float cardW = ImGui::GetContentRegionAvail().x;
            float cardH = ImGui::GetTextLineHeight() * 3.0f +
                          ImGui::GetStyle().FramePadding.y * 4.0f;

            ImGui::PushStyleColor(
                ImGuiCol_ChildBg,
                ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
            ImGui::BeginChild("##card", ImVec2(cardW, cardH),
                              ImGuiChildFlags_Borders);

            if (state.boldFont) ImGui::PushFont(state.boldFont);
            ImGui::Text("%s", lic.name);
            if (state.boldFont) ImGui::PopFont();

            if (lic.version[0] != '\0')
            {
                ImGui::SameLine();
                ImGui::TextDisabled("v%s", lic.version);
            }

            ImGui::TextDisabled("%s", lic.licenseType);

            ImGui::EndChild();
            ImGui::PopStyleColor();

            if (ImGui::IsItemClicked()) { popupIdx = i; }

            if (ImGui::IsItemHovered())
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }

            ImGui::Spacing();
            ImGui::PopID();
        }

        if (popupIdx >= 0 && popupIdx < (int)licenses.size() &&
            popupIdx < (int)state.licenseTexts.size())
        {
            ImGui::OpenPopup("##licensePopup");
            ImVec2 center = ImGui::GetMainViewport()->GetCenter();
            ImGui::SetNextWindowPos(center, ImGuiCond_Appearing,
                                    ImVec2(0.5f, 0.5f));
            ImVec2 displaySize = ImGui::GetMainViewport()->Size;
            ImGui::SetNextWindowSize(
                ImVec2(displaySize.x * 0.7f, displaySize.y * 0.7f),
                ImGuiCond_Appearing);

            const auto& lic = licenses[popupIdx];
            auto& text = state.licenseTexts[popupIdx];
            if (ImGui::BeginPopupModal(
                    "##licensePopup", nullptr,
                    ImGuiWindowFlags_NoSavedSettings))
            {
                if (state.boldFont) ImGui::PushFont(state.boldFont);
                ImGui::Text("%s", lic.name);
                if (state.boldFont) ImGui::PopFont();

                if (lic.version[0] != '\0')
                {
                    ImGui::SameLine();
                    ImGui::TextDisabled("v%s", lic.version);
                }

                ImGui::TextDisabled("%s", lic.licenseType);
                if (lic.url[0] != '\0')
                {
                    ImGui::TextDisabled("%s", lic.url);
                }

                ImGui::Separator();

                ImVec2 textAvail = ImGui::GetContentRegionAvail();
                textAvail.y -= ImGui::GetFrameHeightWithSpacing();

                if (state.monoFont) ImGui::PushFont(state.monoFont);
                ImGui::InputTextMultiline(
                    "##licText", const_cast<char*>(text.c_str()),
                    text.size() + 1, textAvail,
                    ImGuiInputTextFlags_ReadOnly);
                if (state.monoFont) ImGui::PopFont();

                if (ImGui::Button("Close"))
                {
                    popupIdx = -1;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }

        ImGui::EndChild();
    }

}  // namespace shoecomp
