cmake_minimum_required(VERSION 3.28)
project(helloworld_with_helloimgui)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force static linking for all third-party libraries
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

include(ExternalProject)

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
# 2. libpng (needs zlib)
# =====================================================================
set(PNG_SHARED OFF CACHE BOOL "" FORCE)
set(PNG_STATIC ON CACHE BOOL "" FORCE)
set(PNG_TESTS OFF CACHE BOOL "" FORCE)
set(PNG_TOOLS OFF CACHE BOOL "" FORCE)
set(PNG_FRAMEWORK OFF CACHE BOOL "" FORCE)
# Point libpng at our zlib
set(ZLIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib" CACHE PATH "" FORCE)
add_subdirectory(third_party/libpng EXCLUDE_FROM_ALL)

# Write a custom FindPNG.cmake so downstream find_package(PNG) finds our static libpng
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/cmake_modules/FindPNG.cmake" "
if(TARGET png_static AND NOT TARGET PNG::PNG)
    add_library(PNG::PNG IMPORTED INTERFACE GLOBAL)
    target_link_libraries(PNG::PNG INTERFACE png_static)
endif()
set(PNG_FOUND TRUE)
set(PNG_LIBRARIES png_static)
set(PNG_INCLUDE_DIRS
    \"${CMAKE_CURRENT_SOURCE_DIR}/third_party/libpng\"
    \"${CMAKE_CURRENT_BINARY_DIR}/third_party/libpng\")
set(PNG_INCLUDE_DIR \${PNG_INCLUDE_DIRS})
set(PNG_VERSION_STRING \"1.6\")
")

# =====================================================================
# 3. libjpeg-turbo (no deps, ExternalProject — cannot use add_subdirectory)
# =====================================================================
set(_jpeg_build_dir "${CMAKE_CURRENT_BINARY_DIR}/libjpeg-turbo-build")

ExternalProject_Add(libjpeg-turbo-ep
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libjpeg-turbo"
    BINARY_DIR "${_jpeg_build_dir}"
    INSTALL_COMMAND ""
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DENABLE_SHARED=FALSE
        -DENABLE_STATIC=TRUE
        -DWITH_TURBOJPEG=FALSE
        -DWITH_TOOLS=FALSE
        -DWITH_TESTS=FALSE
    BUILD_BYPRODUCTS
        "${_jpeg_build_dir}/${CMAKE_STATIC_LIBRARY_PREFIX}jpeg${CMAKE_STATIC_LIBRARY_SUFFIX}"
)

add_library(jpeg-static STATIC IMPORTED GLOBAL)
set_target_properties(jpeg-static PROPERTIES
    IMPORTED_LOCATION "${_jpeg_build_dir}/${CMAKE_STATIC_LIBRARY_PREFIX}jpeg${CMAKE_STATIC_LIBRARY_SUFFIX}"
    INTERFACE_INCLUDE_DIRECTORIES
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libjpeg-turbo;${_jpeg_build_dir}"
)
add_dependencies(jpeg-static libjpeg-turbo-ep)

# =====================================================================
# 4. freetype (uses zlib + libpng via our Find modules)
# =====================================================================
set(FT_DISABLE_HARFBUZZ TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 TRUE CACHE BOOL "" FORCE)
add_subdirectory(third_party/freetype EXCLUDE_FROM_ALL)

# =====================================================================
# 5. jtjson (no deps)
# =====================================================================
set(JSON_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(DOUBLE_CONVERSION_VENDORED ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/jtjson EXCLUDE_FROM_ALL)

# =====================================================================
# 6. hello_imgui (detects TARGET freetype automatically)
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
# 7. App target
# =====================================================================
include_directories(${CMAKE_SOURCE_DIR}/include)

set(SC_SRCS
    src/ui/mainWindow.cpp
    src/formats/pngLoader.cpp
    src/main.cpp
)
hello_imgui_add_app(hello_world ${SC_SRCS})
target_link_libraries(hello_world PRIVATE json)
target_link_libraries(hello_world PRIVATE jpeg-static)
