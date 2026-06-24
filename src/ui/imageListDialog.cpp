#include "ui/imageListDialog.h"
#include "ui/imageCanvas.h"
#include "ui/uiHelpers.h"
#include "imgui.h"
#include <cstdio>

namespace shoecomp
{
    void ImageListDialog::render(
        std::vector<std::unique_ptr<ImageCanvas>>& images,
        int& viewerLeftIdx, int& viewerRightIdx,
        int& activeGalleryImage)
    {
        if (!popupBeginClosable("Images", show, 0.5f, 0.6f, 0.25f,
                                0.2f))
            return;

        ImGui::TextDisabled("Minimize  Name");
        ImGui::Separator();

        int popRemoveIdx = -1;
        for (int i = 0; i < (int)images.size(); ++i)
        {
            ImGui::PushID(i);
            if (ImGui::Button("X")) popRemoveIdx = i;
            ImGui::SameLine();
            ImGui::Checkbox("Minimize", &images[i]->minimized);
            ImGui::SameLine();
            ImGui::Text("%s", images[i]->name().c_str());
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
