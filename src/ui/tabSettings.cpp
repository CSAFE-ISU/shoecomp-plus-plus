#include "ui/mainWindow.h"
#include "ui/imageCanvas.h"
#include "ui/uiHelpers.h"

namespace shoecomp
{
    void renderFilesAndSettings(AppState& state)
    {
        renderSettingsTab(state.settings, *state.activeProto);
    }

}  // namespace shoecomp
