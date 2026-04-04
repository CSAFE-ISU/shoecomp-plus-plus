#include "ui/imageListDialog.h"
#include "ui/imageCanvas.h"
#include "ui/uiHelpers.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <cstdio>

namespace shoecomp
{
    void ImageListDialog::render(std::vector<ImageCanvas>& images,
                                 int& viewerLeftIdx,
                                 int& viewerRightIdx,
                                 int& activeGalleryImage)
    {
        if (!popupBeginClosable("Images", show, 0.5f, 0.6f, 0.25f,
                                0.2f))
            return;

        int popRemoveIdx = -1;
        for (int i = 0; i < (int)images.size(); ++i)
        {
            ImGui::PushID(i);
            if (ImGui::Button("X")) popRemoveIdx = i;
            ImGui::SameLine();

            char winName[128];
            snprintf(winName, sizeof(winName), "%s###gallery_%d",
                     images[i].image->name.c_str(), i);
            ImGuiWindow* win = ImGui::FindWindowByName(winName);
            bool collapsed = win ? win->Collapsed : false;
            if (ImGui::Checkbox("##min", &collapsed))
            {
                ImGui::SetWindowCollapsed(winName, collapsed);
            }
            ImGui::SameLine();
            ImGui::Text("%s%s", images[i].image->name.c_str(),
                        collapsed ? " (minimized)" : "");
            ImGui::PopID();
        }
        if (popRemoveIdx >= 0)
        {
            images.erase(images.begin() + popRemoveIdx);
            clampViewerIndices(popRemoveIdx, (int)images.size(),
                               viewerLeftIdx, viewerRightIdx,
                               activeGalleryImage);
        }

        if (ImGui::Button("Close")) { ImGui::CloseCurrentPopup(); }

        ImGui::EndPopup();
    }
}  // namespace shoecomp
