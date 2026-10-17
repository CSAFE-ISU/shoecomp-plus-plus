cmake_minimum_required(VERSION 3.15)
project(aligncalc)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/eigen")

add_library(aligncalc
    aligncalc.cpp
    clq/maxClique.cpp
    clq/search.cpp
    models/rigidPoints.cpp
    models/similarity2d.cpp
    models/rigidLines3d.cpp
    models/pointAnchor2d.cpp
    estimate.cpp
    transform.cpp
    rts/triangle2d.cpp
)

target_include_directories(aligncalc PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(aligncalc PUBLIC Eigen3::Eigen)
