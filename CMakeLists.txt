cmake_minimum_required(VERSION 3.28)
project(shoecomp)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force static linking for all third-party libraries
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# Strip unused functions/data to minimize binary size
if(NOT MSVC)
    add_compile_options(-ffunction-sections -fdata-sections)
    if(APPLE)
        add_link_options(-Wl,-dead_strip)
    else()
        add_link_options(-Wl,--gc-sections)
    endif()
endif()

# =====================================================================
# 1. zlib (no deps)
# =====================================================================
set(ZLIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ZLIB_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/zlib EXCLUDE_FROM_ALL)

# Write a custom FindZLIB.cmake so downstream find_package(ZLIB) finds our static zlib
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/cmake_modules/FindZLIB.cmake" "
if(TARGET zlibstatic AND NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB IMPORTED INTERFACE GLOBAL)
    target_link_libraries(ZLIB::ZLIB INTERFACE zlibstatic)
endif()
set(ZLIB_FOUND TRUE)
set(ZLIB_LIBRARIES zlibstatic)
set(ZLIB_INCLUDE_DIRS
    \"${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib\"
    \"${CMAKE_CURRENT_BINARY_DIR}/third_party/zlib\")
set(ZLIB_INCLUDE_DIR \${ZLIB_INCLUDE_DIRS})
")
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_BINARY_DIR}/cmake_modules")

# =====================================================================
# 2. freetype (uses zlib, harfbuzz/brotli/bzip2/png disabled)
# =====================================================================
set(FT_DISABLE_HARFBUZZ TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG TRUE CACHE BOOL "" FORCE)
add_subdirectory(third_party/freetype EXCLUDE_FROM_ALL)

# =====================================================================
# 3. jtjson (no deps)
# =====================================================================
set(JSON_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(DOUBLE_CONVERSION_VENDORED ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/jtjson EXCLUDE_FROM_ALL)

# =====================================================================
# 3b. libpng (uses zlib)
# =====================================================================
set(PNG_SHARED OFF CACHE BOOL "" FORCE)
set(PNG_STATIC ON CACHE BOOL "" FORCE)
set(PNG_TESTS OFF CACHE BOOL "" FORCE)
set(PNG_TOOLS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/libpng EXCLUDE_FROM_ALL)

# =====================================================================
# 4. hello_imgui (detects TARGET freetype automatically)
# =====================================================================
set(HELLOIMGUI_DOWNLOAD_FREETYPE_IF_NEEDED OFF CACHE BOOL "" FORCE)
set(HELLOIMGUI_USE_FREETYPE ON CACHE BOOL "" FORCE)
if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/third_party/hello_imgui)
    add_subdirectory(third_party/hello_imgui EXCLUDE_FROM_ALL)
endif()

if (NOT TARGET hello_imgui)
    message(FATAL_ERROR "couldn't find hello_imgui, submodule missing?")
endif()

# =====================================================================
# 5. AlignCalc (alignment calculations)
# =====================================================================
add_subdirectory(third_party/eigen EXCLUDE_FROM_ALL)
add_subdirectory(third_party/aligncalc EXCLUDE_FROM_ALL)

# =====================================================================
# 6. App target
# =====================================================================
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party
)

set(SC_SRCS
    src/ui/mainWindow.cpp
    src/ui/imageCanvas.cpp
    src/ui/uiHelpers.cpp
    src/ui/settings.cpp
    src/ui/errorPopup.cpp
    src/ui/loadBrowser.cpp
    src/ui/saveBrowser.cpp
    src/ui/imageListDialog.cpp
    src/ui/alignDialog.cpp
    src/ui/syncViews.cpp
    src/calc/align.cpp
    src/calc/rts_align.cpp
    src/formats/pngLoader.cpp
    src/formats/pngSaver.cpp
    src/formats/jpegLoader.cpp
    src/formats/annotationIo.cpp
    src/main.cpp
)
hello_imgui_add_app(shoecomp ${SC_SRCS})
target_link_libraries(shoecomp PRIVATE json aligncalc png_static)
