cmake_minimum_required(VERSION 3.5)
project(matrix_read C CXX)

# Enable extra warnings. Not needed but keeps consistency
include (../cmake/FatalWarnings.cmake)
ADM_EXTRA_WARNINGS()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(../cpp/driver driver)

find_package(Threads)
find_library (WIRINGPI_LIB NAMES wiringPi)
find_library (WIRINGPI_DEV_LIB NAMES wiringPiDev)
find_library (CRYPT_LIB NAMES crypt)
find_library (GFLAGS_LIB NAMES gflags)
message(STATUS "gflags found =>" "${GFLAGS_LIB}")

add_executable(matrix_read
  matrix_read.cpp
  audio_processor.cpp
)

set_property(TARGET matrix_read PROPERTY CXX_STANDARD 17)

target_link_libraries(matrix_read matrix_creator_hal)
target_link_libraries(matrix_read ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(matrix_read ${WIRINGPI_LIB} ${WIRINGPI_DEV_LIB} ${CRYPT_LIB})
target_link_libraries(matrix_read ${GFLAGS_LIB})

install(TARGETS matrix_read DESTINATION bin)

