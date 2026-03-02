#include "ui.h"

namespace shoecomp
{
    void guiFunction(void)
    {
        ImGui::Text("Hello, ");     // Display a simple label
        if (ImGui::Button("Bye!"))  // Display a button
                                    // and immediately handle its action
                                    // if it is clicked!
            HelloImGui::GetRunnerParams()->appShallExit = true;
    }

    void submain(void) { HelloImGui::Run(guiFunction, "Hello", true); }

}  // namespace shoecomp

