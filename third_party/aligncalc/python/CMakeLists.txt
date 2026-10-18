# Python extension aligncalc._core, built by scikit-build-core (see
# pyproject.toml): the aligncalc library from the parent directory,
# static and with hidden symbols, linked into the module.
cmake_minimum_required(VERSION 3.23)
project(aligncalc_python LANGUAGES CXX)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(ALIGNCALC_SHARED OFF CACHE BOOL "" FORCE)
set(ALIGNCALC_INSTALL OFF CACHE BOOL "" FORCE)
set(ALIGNCALC_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(.. aligncalc EXCLUDE_FROM_ALL)

find_package(Python 3.8 REQUIRED COMPONENTS Interpreter Development.Module)
find_package(pybind11 CONFIG REQUIRED)

pybind11_add_module(_core MODULE bindings.cpp)
target_link_libraries(_core PRIVATE aligncalc::aligncalc)
get_target_property(ALIGNCALC_PY_VERSION aligncalc VERSION)
target_compile_definitions(_core PRIVATE
    ALIGNCALC_PY_VERSION="${ALIGNCALC_PY_VERSION}")

# Only the module goes into the wheel (pyproject: install.components);
# the bundled Eigen's own install rules stay out.
install(TARGETS _core LIBRARY DESTINATION aligncalc COMPONENT python)
