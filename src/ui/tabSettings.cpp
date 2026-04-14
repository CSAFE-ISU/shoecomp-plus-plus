#include "ui/mainWindow.h"
#include "ui/imageCanvas.h"
#include "ui/uiHelpers.h"

namespace shoecomp
{
    void renderSettings(AppState& state)
    {
        ImGui::Text("Loaded images:");
        int removeIdx = -1;
        for (int i = 0; i < (int)state.images.size(); ++i)
        {
            ImGui::PushID(i);
            if (ImGui::Button("X")) removeIdx = i;
            ImGui::SameLine();
            ImGui::Text("%s", state.images[i].image->name.c_str());
            ImGui::PopID();
        }
        if (removeIdx >= 0)
        {
            state.images.erase(state.images.begin() + removeIdx);
            int dummy = -1;
            clampViewerIndices(removeIdx, (int)state.images.size(),
                               state.viewerLeftIdx,
                               state.viewerRightIdx, dummy);
        }
    }

    void renderFilesAndSettings(AppState& state)
    {
        renderSettingsTab(state.settings);
    }

}  // namespace shoecomp
