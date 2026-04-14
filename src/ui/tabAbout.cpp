#include "ui/mainWindow.h"
#include "hello_imgui/hello_imgui.h"

namespace shoecomp
{
    void renderAbout(AppState& state)
    {
        static std::string aboutText;
        if (aboutText.empty())
        {
            auto data = HelloImGui::LoadAssetFileData("about.txt");
            if (data.data)
            {
                aboutText.assign((const char*)data.data, data.dataSize);
                HelloImGui::FreeAssetFileData(&data);
            }
        }

        ImVec2 avail = ImGui::GetContentRegionAvail();
        if (state.monoFont) ImGui::PushFont(state.monoFont);
        ImGui::InputTextMultiline(
            "##about", const_cast<char*>(aboutText.c_str()),
            aboutText.size() + 1, avail, ImGuiInputTextFlags_ReadOnly);
        if (state.monoFont) ImGui::PopFont();
    }

}  // namespace shoecomp
