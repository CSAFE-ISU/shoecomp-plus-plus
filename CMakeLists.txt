cmake_minimum_required(VERSION 3.28)
project(helloworld_with_helloimgui)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build hello_imgui
# =================
if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/third_party/hello_imgui)
    add_subdirectory(third_party/hello_imgui EXCLUDE_FROM_ALL)
endif()

if (NOT TARGET hello_imgui)
    message(FATAL_ERROR "couldn't find hello_imgui, submodule missing?")
endif()

# Build your app
# ==============
# hello_imgui_add_app is a helper function, similar to cmake's "add_executable"
# Usage:
# hello_imgui_add_app(app_name file1.cpp file2.cpp ...)
#
# Features:
# * It will automatically link the target to the required libraries (hello_imgui, OpenGl, glad, etc)
# * It will embed the assets (for desktop, mobile, and emscripten apps)
# * It will perform additional customization (app icon and name on mobile platforms, etc)

include_directories(${CMAKE_SOURCE_DIR}/include)

# portable installation is the easiest way to install the app
set(SC_SRCS
    src/ui/mainWindow.cpp
    src/main.cpp
)
hello_imgui_add_app(hello_world ${SC_SRCS})
