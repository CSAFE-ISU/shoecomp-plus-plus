#include "ui/errorPopup.h"
#include "ui/uiHelpers.h"
#include "imgui.h"

namespace shoecomp
{
    void ErrorPopup::render()
    {
        if (!popupBeginClosable(title.c_str(), show, 0.5f, 0.3f, 0.25f,
                                0.35f))
            return;
        ImGui::TextWrapped("%s", message.c_str());
        if (ImGui::Button("OK")) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
}  // namespace shoecomp
