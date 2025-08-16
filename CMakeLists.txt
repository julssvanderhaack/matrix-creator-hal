cmake_minimum_required(VERSION 3.15)
project(matrix_creator_hal LANGUAGES C CXX)
# Add -Wall and -Wextra. Also,
# treat C/C++ warnings as errors if -DADM_FATAL_WARNINGS=ON.
include (cmake/FatalWarnings.cmake)
ADM_EXTRA_WARNINGS()

option(BUILD_PYTHON "Build python bindings with pybind " ON)
# add_subdirectory(demos)
add_subdirectory(tfg)

