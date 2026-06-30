cmake_minimum_required(VERSION 3.28)
include(ExternalProject)
#
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
    src/ui/imageCanvas2d.cpp
    src/ui/imageCanvasKinds.cpp
    src/ui/shoeCanvas.cpp
    src/ui/ebtsCanvas.cpp
    src/ui/uiHelpers.cpp
    src/ui/settings.cpp
    src/ui/tabImageViewer.cpp
    src/ui/tabImageComparison.cpp
    src/ui/tabSettings.cpp
    src/ui/tabAbout.cpp
    src/ui/licenseData.cpp
    src/ui/errorPopup.cpp
    src/ui/loadBrowser.cpp
    src/ui/saveBrowser.cpp
    src/ui/imageListDialog.cpp
    src/ui/alignDialog.cpp
    src/ui/syncViews.cpp
    src/ui/splash.cpp
    src/calc/align.cpp
    src/calc/rts_align.cpp
    src/formats/pngLoader.cpp
    src/formats/pngSaver.cpp
    src/formats/jpegLoader.cpp
    src/formats/EBTSLoader.cpp
    src/formats/annotationIo.cpp
    src/formats/platforms.cpp
    src/main.cpp
)

# =====================================================================
# 7. Embedded static assets (generated via binary_to_compressed_c)
# =====================================================================
if (NOT EMSCRIPTEN)
    add_executable(binary_to_compressed_c
        third_party/hello_imgui/external/imgui/misc/fonts/binary_to_compressed_c.cpp)
    set_target_properties(binary_to_compressed_c PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    set(B2CC_TOOL $<TARGET_FILE:binary_to_compressed_c>)
    set(B2CC_DEP  binary_to_compressed_c)
else()
    # Build a native host tool even when cross-compiling for Emscripten
    find_program(HOST_CXX NAMES c++ g++ clang++)
    if(NOT HOST_CXX)
        message(FATAL_ERROR
            "No native C++ compiler found for host tool build. "
            "Set HOST_CXX to a native compiler path.")
    endif()
    set(_B2CC_SRC
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/hello_imgui/external/imgui/misc/fonts/binary_to_compressed_c.cpp")
    set(_B2CC_BIN
        "${CMAKE_CURRENT_BINARY_DIR}/binary_to_compressed_c")
    add_custom_command(
        OUTPUT  ${_B2CC_BIN}
        COMMAND ${HOST_CXX} ${_B2CC_SRC} -o ${_B2CC_BIN}
        DEPENDS ${_B2CC_SRC}
        COMMENT "Building native binary_to_compressed_c"
        VERBATIM
    )
    add_custom_target(binary_to_compressed_c DEPENDS ${_B2CC_BIN})
    set(B2CC_TOOL ${_B2CC_BIN})
    set(B2CC_DEP  binary_to_compressed_c)
endif()

set(SC_GENERATED_ASSETS_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated_assets)
file(MAKE_DIRECTORY ${SC_GENERATED_ASSETS_DIR})

set(SC_ASSET_SPECS
    "assets/fonts/Montserrat-Regular.ttf|MontserratRegular|compress"
    "assets/fonts/Montserrat-SemiBold.ttf|MontserratSemiBold|compress"
    "assets/fonts/Inconsolata-Regular.ttf|InconsolataRegular|compress"
    "assets/fonts/icon.png|ShoeCompIcon|raw"
    "third_party/hello_imgui/external/imgui/LICENSE.txt|LicDearImGui|raw"
    "third_party/hello_imgui/LICENSE|LicHelloImGui|raw"
    "third_party/aligncalc/eigen/COPYING.MPL2|LicEigen|raw"
    "third_party/freetype/docs/FTL.TXT|LicFreetype|raw"
    "third_party/jtjson/LICENSE|LicJsonCpp|raw"
    "third_party/libpng/LICENSE|LicLibpng|raw"
    "third_party/zlib/LICENSE|LicZlib|raw"
    "third_party/libjpeg-turbo/LICENSE.md|LicLibjpegTurbo|raw"
    "third_party/jtjson/double-conversion/LICENSE|LicDoubleConversion|raw"
    "assets/fonts/Inconsolata-OFL.txt|LicInconsolata|raw"
    "assets/fonts/Montserrat-OFL.txt|LicMontserrat|raw"
)
set(SC_ASSET_SCRIPT
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/strip_compressed_suffix.cmake)
set(SC_GENERATED_ASSET_SRCS)
foreach(spec IN LISTS SC_ASSET_SPECS)
    string(REPLACE "|" ";" parts ${spec})
    list(GET parts 0 src_rel)
    list(GET parts 1 sym)
    list(GET parts 2 mode)
    set(out_c ${SC_GENERATED_ASSETS_DIR}/${sym}.cpp)
    add_custom_command(
        OUTPUT ${out_c}
        COMMAND ${CMAKE_COMMAND}
                -DTOOL=${B2CC_TOOL}
                -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/${src_rel}
                -DSYMBOL=${sym}
                -DOUTPUT=${out_c}
                -DMODE=${mode}
                -P ${SC_ASSET_SCRIPT}
        DEPENDS ${B2CC_DEP}
                ${CMAKE_CURRENT_SOURCE_DIR}/${src_rel}
                ${SC_ASSET_SCRIPT}
        VERBATIM
    )
    list(APPEND SC_GENERATED_ASSET_SRCS ${out_c})
endforeach()

list(APPEND SC_SRCS ${SC_GENERATED_ASSET_SRCS})

hello_imgui_add_app(shoecomp ${SC_SRCS})
target_link_libraries(shoecomp PRIVATE json aligncalc png_static)
