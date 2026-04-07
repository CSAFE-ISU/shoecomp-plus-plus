#ifndef SHOECOMP_UI_HELPERS_H
#define SHOECOMP_UI_HELPERS_H

#include "imgui.h"
#include "jtjson/json.h"
#include <string>
#include <vector>

namespace shoecomp
{
    static constexpr float kDegToRad = 3.14159265358979f / 180.0f;

    // UI sizing constants (multipliers relative to font size)
    // Rounding
    static constexpr float kWindowRounding = 0.3f;
    static constexpr float kChildRounding = 0.25f;
    static constexpr float kFrameRounding = 0.15f;
    static constexpr float kPopupRounding = 0.25f;
    static constexpr float kScrollbarRounding = 0.3f;
    static constexpr float kGrabRounding = 0.15f;
    static constexpr float kTabRounding = 0.15f;

    // Borders
    static constexpr float kBorderSize = 0.05f;

    // Padding
    static constexpr float kWindowPadding = 0.5f;
    static constexpr float kFramePaddingX = 0.35f;
    static constexpr float kFramePaddingY = 0.2f;
    static constexpr float kCellPaddingX = 0.25f;
    static constexpr float kCellPaddingY = 0.15f;
    static constexpr float kSafeAreaPadding = 0.15f;

    // Spacing
    static constexpr float kItemSpacingX = 0.35f;
    static constexpr float kItemSpacingY = 0.25f;
    static constexpr float kItemInnerSpacingX = 0.25f;
    static constexpr float kItemInnerSpacingY = 0.15f;
    static constexpr float kIndentSpacing = 1.0f;

    // Interactive elements
    static constexpr float kScrollbarSize = 0.65f;
    static constexpr float kGrabMinSize = 0.5f;

    // Window constraints
    static constexpr float kWindowMinWidth = 10.0f;
    static constexpr float kWindowMinHeight = 5.0f;

    // Splash screen layout
    static constexpr float kSplashButtonSize = 2.5f;
    static constexpr float kSplashButtonOffsetX = 0.5f;
    static constexpr float kSplashButtonOffsetY = 0.75f;
    static constexpr float kSplashLeftMargin = 2.5f;
    static constexpr float kSplashIconSize = 3.0f;
    static constexpr float kSplashIconSpacing = 0.75f;
    static constexpr float kSplashTitleOffsetY = 2.0f;
    static constexpr float kSplashProgressHeight = 0.2f;
    static constexpr float kSplashProgressOffsetY = 0.5f;
    static constexpr float kSplashStepTextOffsetY = 1.0f;

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
