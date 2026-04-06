#ifndef SHOECOMP_UI_HELPERS_H
#define SHOECOMP_UI_HELPERS_H

#include "imgui.h"
#include "jtjson/json.h"
#include <string>
#include <vector>

namespace shoecomp
{
    static constexpr float kDegToRad = 3.14159265358979f / 180.0f;

    // Opens a sized/positioned modal popup triggered by a
    // show-then-hide flag. Returns false if popup isn't
    // open (caller should return). Caller still calls
    // EndPopup().
    bool popupBeginClosable(const char* title, bool& show, float wRatio,
                            float hRatio, float xOff, float yOff);

    // Checks annotations[key] exists and is an array.
    bool hasAnnotationArray(jt::Json& annotations, const char* key);

    // Emits a settings table row: label in col 0,
    // skips col 1, advances to col 2 with full-width
    // next item.
    void settingsTableRow(const char* label);

    // Clamps viewer indices after an image is removed.
    void clampViewerIndices(int removedIdx, int imageCount,
                            int& leftIdx, int& rightIdx,
                            int& activeIdx);

    // Refreshes a directory listing for file browsers.
    void refreshDirEntries(const std::string& dir,
                           const std::string& extension,
                           std::vector<std::string>& entries,
                           bool& needsRefresh);

    // Navigates to a directory (try-catch wrapper).
    void navigateDir(std::string& currentDir,
                     const std::string& relativePath,
                     bool& needsRefresh);

}  // namespace shoecomp

#endif
