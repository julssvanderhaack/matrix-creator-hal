# Need pybind11 installed

# To use this is recommended
# This often is a good way to get the current Python, works in environments:
# cmake -DPYTHON_EXECUTABLE=$(python3 -c "import sys; print(sys.executable)") ..

cmake_minimum_required(VERSION 3.15)
project(${SKBUILD_PROJECT_NAME} LANGUAGES CXX C)


# Enable extra warnings. Not needed but keeps consistency
include (../../../cmake/FatalWarnings.cmake)
ADM_EXTRA_WARNINGS()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(../../../cpp/driver driver)

add_definitions(-std=c++17)
find_package(Threads)

set(PYBIND11_FINDPYTHON ON)
find_package(pybind11 CONFIG REQUIRED)

find_library (WIRINGPI_LIB NAMES wiringPi)
find_library (WIRINGPI_DEV_LIB NAMES wiringPiDev)
find_library (CRYPT_LIB NAMES crypt)
find_library (GFLAGS_LIB NAMES gflags)

pybind11_add_module(matrix_pybind matrix_python.cpp)
set_property(TARGET matrix_pybind PROPERTY CXX_STANDARD 11)

target_link_libraries(matrix_pybind PRIVATE matrix_creator_hal_static)
target_link_libraries(matrix_pybind PRIVATE ${CMAKE_THREAD_LIBS_INIT} )
target_link_libraries(matrix_pybind PRIVATE ${WIRINGPI_LIB} ${WIRINGPI_DEV_LIB} ${CRYPT_LIB})
target_link_libraries(matrix_pybind PRIVATE ${GFLAGS_LIB})
target_link_libraries(matrix_pybind PRIVATE paho-mqttpp3 paho-mqtt3as)

# install(TARGETS matrix_pybind DESTINATION .)
install(TARGETS matrix_pybind LIBRARY DESTINATION .)




