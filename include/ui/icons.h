#ifndef SHOECOMP_UI_ICONS
#define SHOECOMP_UI_ICONS

// Material Icons codepoints (subset), matching the glyphs in
// assets/fonts/MaterialIcons-Regular.ttf. Values follow the widely
// used IconFontCppHeaders mapping. Use with the dedicated icon font
// (AppState::iconFont). u8"" keeps the encoding UTF-8 regardless of the
// compiler's execution charset, which is what ImGui expects.

namespace shoecomp
{
    // Glyph range to load from the Material Icons font.
    static constexpr unsigned short kIconRangeMin = 0xe000;
    static constexpr unsigned short kIconRangeMax = 0xf8ff;
}  // namespace shoecomp

#define ICON_MD_ADD u8""
#define ICON_MD_FIBER_MANUAL_RECORD u8""
#define ICON_MD_CROP_SQUARE u8""
#define ICON_MD_CROP_FREE u8""
#define ICON_MD_UNDO u8""
#define ICON_MD_CLEAR u8""
#define ICON_MD_DELETE u8""
#define ICON_MD_LOCK u8""
#define ICON_MD_LOCK_OPEN u8""
#define ICON_MD_NAVIGATE_BEFORE u8""
#define ICON_MD_NAVIGATE_NEXT u8""
#define ICON_MD_EDIT u8""
#define ICON_MD_SAVE u8""
#define ICON_MD_FOLDER_OPEN u8""
#define ICON_MD_PHOTO_LIBRARY u8""
#define ICON_MD_IMAGE u8""
#define ICON_MD_STRAIGHTEN u8""
#define ICON_MD_TUNE u8""

#endif
