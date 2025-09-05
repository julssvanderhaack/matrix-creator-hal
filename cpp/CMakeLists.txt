cmake_minimum_required(VERSION 3.5)

# Add -Wall and -Wextra. Also,
# treat C/C++ warnings as errors if -DADM_FATAL_WARNINGS=ON.
include (../cmake/FatalWarnings.cmake)
ADM_EXTRA_WARNINGS()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(driver)
