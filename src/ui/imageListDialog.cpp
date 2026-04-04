#include "ui/imageListDialog.h"
#include "ui/imageCanvas.h"
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
        if (show)
        {
            ImGui::OpenPopup("Images");
            show = false;
        }

        ImVec2 ds = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowSize(ImVec2(ds.x * 0.5f, ds.y * 0.6f),
                                 ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(ds.x * 0.25f, ds.y * 0.2f),
                                ImGuiCond_Always);

        bool imagesOpen = true;
        if (!ImGui::BeginPopupModal("Images", &imagesOpen,
                                    ImGuiWindowFlags_NoResize))
            return;
        if (!imagesOpen)
        {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }

        int popRemoveIdx = -1;
        for (int i = 0; i < (int)images.size(); ++i)
        {
            ImGui::PushID(i);
            if (ImGui::Button("X")) popRemoveIdx = i;
            ImGui::SameLine();

            // Read collapsed state from the
            // gallery window via ImGui
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
            if (viewerLeftIdx >= (int)images.size())
                viewerLeftIdx = (int)images.size() - 1;
            if (viewerRightIdx >= (int)images.size())
                viewerRightIdx = (int)images.size() - 1;
            if (activeGalleryImage == popRemoveIdx)
                activeGalleryImage = -1;
            else if (activeGalleryImage > popRemoveIdx)
                activeGalleryImage--;
        }

        if (ImGui::Button("Close")) { ImGui::CloseCurrentPopup(); }

        ImGui::EndPopup();
    }
}  // namespace shoecomp
