#include "ui/errorPopup.h"
#include "imgui.h"

namespace shoecomp
{
    void ErrorPopup::render()
    {
        if (show)
        {
            ImGui::OpenPopup(title.c_str());
            show = false;
        }
        ImVec2 ds = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowSize(
            ImVec2(ds.x * 0.5f, ds.y * 0.3f),
            ImGuiCond_Always);
        ImGui::SetNextWindowPos(
            ImVec2(ds.x * 0.25f, ds.y * 0.35f),
            ImGuiCond_Always);
        bool errOpen = true;
        if (ImGui::BeginPopupModal(
                title.c_str(), &errOpen,
                ImGuiWindowFlags_NoResize))
        {
            ImGui::TextWrapped("%s", message.c_str());
            if (ImGui::Button("OK"))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
}  // namespace shoecomp
