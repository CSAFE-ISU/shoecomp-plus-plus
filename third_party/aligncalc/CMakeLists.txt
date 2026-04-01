cmake_minimum_required(VERSION 3.15)
project(aligncalc)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# TODO: better way to find eigen
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../eigen")
find_package (Eigen3 REQUIRED NO_MODULE)

add_library(aligncalc
    clqmtch.cpp
    graphInfo.cpp
    aligncalc.cpp
    rtsUtils.cpp
    triangle2d.cpp
)
target_include_directories(aligncalc PUBLIC ${Eigen3_SOURCE_DIR})
set_target_properties(aligncalc PROPERTIES PUBLIC_HEADER "aligncalc.h")
set_target_properties(aligncalc PROPERTIES PUBLIC_HEADER "workerChannel.h")
set_target_properties(aligncalc PROPERTIES PUBLIC_HEADER "multiQueue.h")
