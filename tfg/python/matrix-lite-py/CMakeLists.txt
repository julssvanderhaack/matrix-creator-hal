cmake_minimum_required(VERSION 3.15)
project(${SKBUILD_PROJECT_NAME} LANGUAGES CXX C)


# Enable extra warnings. Not needed but keeps consistency
include (../../../cmake/FatalWarnings.cmake)
ADM_EXTRA_WARNINGS()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(../../../cpp/driver driver)

find_package(Threads)

set(PYBIND11_FINDPYTHON ON)
find_package(pybind11 CONFIG REQUIRED)

find_library (WIRINGPI_LIB NAMES wiringPi)
find_library (WIRINGPI_DEV_LIB NAMES wiringPiDev)

pybind11_add_module(
 matrix_pybind_bindings
 hal_wrapper/matrix.cpp
 hal_wrapper/drivers/everloop.cpp
 hal_wrapper/drivers/gpio.cpp
 hal_wrapper/drivers/info.cpp
 hal_wrapper/drivers/sensors.cpp
 hal_wrapper/drivers/microphones.cpp
)
set_property(TARGET matrix_pybind_bindings PROPERTY CXX_STANDARD 11)

target_link_libraries(matrix_pybind_bindings PRIVATE matrix_creator_hal_static)
target_link_libraries(matrix_pybind_bindings PRIVATE ${CMAKE_THREAD_LIBS_INIT} )
target_link_libraries(matrix_pybind_bindings PRIVATE ${WIRINGPI_LIB} ${WIRINGPI_DEV_LIB} ${CRYPT_LIB})

install(TARGETS matrix_pybind_bindings LIBRARY DESTINATION .)




