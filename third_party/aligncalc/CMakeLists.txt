cmake_minimum_required(VERSION 3.15)
project(aligncalc)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/eigen")

add_library(aligncalc
    aligncalc.cpp
    clqmtch.cpp
    graphInfo.cpp
    rtsUtils.cpp
    transform.cpp
    triangle2d.cpp
)

target_include_directories(aligncalc PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/eigen")
target_link_libraries(aligncalc PRIVATE Eigen3::Eigen)
set_target_properties(aligncalc PROPERTIES PUBLIC_HEADER "aligncalc.h")
set_target_properties(aligncalc PROPERTIES PUBLIC_HEADER "workerChannel.h")
set_target_properties(aligncalc PROPERTIES PUBLIC_HEADER "multiQueue.h")
