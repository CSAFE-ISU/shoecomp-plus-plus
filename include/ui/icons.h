#ifndef SHOECOMP_UI_ICONS
#define SHOECOMP_UI_ICONS

// Material Icons codepoints (subset), matching the glyphs in
// assets/fonts/MaterialIcons-Regular.ttf. Values follow the widely
// used IconFontCppHeaders mapping. Each macro is a universal-character
// escape (ASCII source, no literal glyphs); the compiler encodes it to
// the UTF-8 ImGui expects. Use with the icon font (AppState::iconFont).

namespace shoecomp
{
    // Glyph range to load from the Material Icons font.
    static constexpr unsigned short kIconRangeMin = 0xe000;
    static constexpr unsigned short kIconRangeMax = 0xf8ff;
}  // namespace shoecomp

#define ICON_MD_ADD u8"\ue145"
#define ICON_MD_FIBER_MANUAL_RECORD u8"\ue061"
#define ICON_MD_CROP_SQUARE u8"\ue3b6"
#define ICON_MD_CROP_FREE u8"\ue3c2"
#define ICON_MD_UNDO u8"\ue166"
#define ICON_MD_CLEAR u8"\ue14c"
#define ICON_MD_DELETE u8"\ue872"
#define ICON_MD_LOCK u8"\ue897"
#define ICON_MD_LOCK_OPEN u8"\ue898"
#define ICON_MD_NAVIGATE_BEFORE u8"\ue408"
#define ICON_MD_NAVIGATE_NEXT u8"\ue409"
#define ICON_MD_EDIT u8"\ue3c9"
#define ICON_MD_SAVE u8"\ue161"
#define ICON_MD_FOLDER_OPEN u8"\ue2c8"
#define ICON_MD_PHOTO_LIBRARY u8"\ue413"
#define ICON_MD_IMAGE u8"\ue3f4"
#define ICON_MD_STRAIGHTEN u8"\ue41c"
#define ICON_MD_TUNE u8"\ue429"
#define ICON_MD_HOME u8"\ue88a"
#define ICON_MD_ZOOM_IN u8"\ue8ff"
#define ICON_MD_ZOOM_OUT u8"\ue900"
#define ICON_MD_ROTATE_RIGHT u8"\ue41a"
#define ICON_MD_ARROW_DROP_DOWN u8"\ue5c5"

#endif
