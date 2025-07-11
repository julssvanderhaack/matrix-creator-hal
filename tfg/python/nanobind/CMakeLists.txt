cmake_minimum_required(VERSION 3.15...3.27)
project(${SKBUILD_PROJECT_NAME} LANGUAGES CXX)

# Enable extra warnings. Not needed but keeps consistency
include (../../../cmake/FatalWarnings.cmake)
ADM_EXTRA_WARNINGS()

add_subdirectory(../../../cpp/driver driver)

if (CMAKE_VERSION VERSION_LESS 3.18)
  set(DEV_MODULE Development)
else()
  set(DEV_MODULE Development.Module)
endif()

find_package(Python 3.8 COMPONENTS Interpreter ${DEV_MODULE} REQUIRED)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()


# Detect the installed nanobind package and import it into CMake
execute_process(
  COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
  OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE nanobind_ROOT)
find_package(nanobind CONFIG REQUIRED)

find_package(Threads)

find_library (WIRINGPI_LIB NAMES wiringPi)
find_library (WIRINGPI_DEV_LIB NAMES wiringPiDev)
find_library (CRYPT_LIB NAMES crypt)
find_library (GFLAGS_LIB NAMES gflags)

# nanobind_add_module(matrix_nanobind matrix_nanobind.cpp)
nanobind_add_module(matrix_nanobind NB_STATIC matrix_nanobind.cpp)
set_property(TARGET matrix_nanobind PROPERTY CXX_STANDARD 17)

target_link_libraries(matrix_nanobind PRIVATE matrix_creator_hal_static)
target_link_libraries(matrix_nanobind PRIVATE ${CMAKE_THREAD_LIBS_INIT} )
target_link_libraries(matrix_nanobind PRIVATE ${WIRINGPI_LIB} ${WIRINGPI_DEV_LIB} ${CRYPT_LIB})
target_link_libraries(matrix_nanobind PRIVATE ${GFLAGS_LIB})
target_link_libraries(matrix_nanobind PRIVATE paho-mqttpp3 paho-mqtt3as)

install(TARGETS matrix_nanobind LIBRARY DESTINATION .)

