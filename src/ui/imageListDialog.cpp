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

        int popRemoveIdx = -1;
        if (ImGui::BeginTable(
                "##imglist", 3,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
        {
            ImGui::TableSetupColumn("##remove",
                                    ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Minimize",
                                    ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Name",
                                    ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (int i = 0; i < (int)images.size(); ++i)
            {
                ImGui::PushID(i);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (ImGui::Button("X")) popRemoveIdx = i;
                ImGui::TableSetColumnIndex(1);
                ImGui::Checkbox("##min", &images[i]->minimized);
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(images[i]->name().c_str());
                ImGui::PopID();
            }
            ImGui::EndTable();
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
