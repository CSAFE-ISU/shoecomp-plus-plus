cmake_minimum_required(VERSION 3.23)
# Version and license follow shoecomp's; shoecomp's CMakeLists checks
# that the two versions match, so bump both together.
project(aligncalc VERSION 0.0.8 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ALIGNCALC_SYSTEM_EIGEN
    "Use an installed Eigen3 instead of the bundled eigen/ submodule" OFF)
option(ALIGNCALC_INSTALL
    "Generate install rules and the aligncalc CMake package"
    ${PROJECT_IS_TOP_LEVEL})

if(NOT ALIGNCALC_SYSTEM_EIGEN
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/eigen/CMakeLists.txt")
    # Installing aligncalc installs the bundled Eigen with its CMake
    # package, which aligncalcConfig.cmake finds.
    if(ALIGNCALC_INSTALL)
        set(EIGEN_BUILD_CMAKE_PACKAGE ON CACHE BOOL "" FORCE)
    endif()
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/eigen")
else()
    find_package(Eigen3 REQUIRED CONFIG)
endif()

# Static on purpose: shoecomp/scpp link aligncalc statically.
add_library(aligncalc STATIC
    src/aligncalc.cpp
    src/clq/maxClique.cpp
    src/clq/search.cpp
    src/models/rigidPoints.cpp
    src/models/similarity2d.cpp
    src/models/rigidLines3d.cpp
    src/models/pointAnchor2d.cpp
    src/estimate.cpp
    src/rts/triangle2d.cpp
)
add_library(aligncalc::aligncalc ALIAS aligncalc)

target_sources(aligncalc PUBLIC
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
        include/aligncalc/alignTypes.h
        include/aligncalc/alignWithRules.h
        include/aligncalc/aligncalc.h
        include/aligncalc/correspondence.h
        include/aligncalc/distinct.h
        include/aligncalc/estimate.h
        include/aligncalc/graphBuild.h
        include/aligncalc/pointAnchor2d.h
        include/aligncalc/progress.h
        include/aligncalc/rigidLines3d.h
        include/aligncalc/rigidPoints.h
        include/aligncalc/rules.h
        include/aligncalc/similarity2d.h
)
target_include_directories(aligncalc PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(aligncalc PUBLIC cxx_std_17)
target_link_libraries(aligncalc PUBLIC Eigen3::Eigen)
if(NOT EMSCRIPTEN)
    find_package(Threads REQUIRED)
    target_link_libraries(aligncalc PUBLIC Threads::Threads)
endif()
set_target_properties(aligncalc PROPERTIES
    VERSION ${PROJECT_VERSION}
    EXPORT_NAME aligncalc)

if(ALIGNCALC_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)
    set(ALIGNCALC_CMAKE_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/aligncalc")

    install(TARGETS aligncalc EXPORT aligncalcTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
    install(EXPORT aligncalcTargets
        NAMESPACE aligncalc::
        DESTINATION "${ALIGNCALC_CMAKE_DIR}")

    configure_package_config_file(
        cmake/aligncalcConfig.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/aligncalcConfig.cmake"
        INSTALL_DESTINATION "${ALIGNCALC_CMAKE_DIR}")
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/aligncalcConfigVersion.cmake"
        COMPATIBILITY SameMinorVersion)
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/aligncalcConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/aligncalcConfigVersion.cmake"
        DESTINATION "${ALIGNCALC_CMAKE_DIR}")
endif()
