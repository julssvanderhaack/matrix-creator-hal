project(matrix_read C CXX)
cmake_minimum_required(VERSION 2.8)

add_definitions(-std=c++11)

# Enable extra warnings. Not needed but keeps consistency
include (../cmake/FatalWarnings.cmake)
ADM_EXTRA_WARNINGS()

find_package(Threads)
find_library (FFTW_LIBRARIES NAMES fftw3f )
find_library (WIRINGPI_LIB NAMES wiringPi)
find_library (WIRINGPI_DEV_LIB NAMES wiringPiDev)
find_library (CRYPT_LIB NAMES crypt)
find_library (GFLAGS_LIB NAMES gflags)
message(STATUS "gflags found =>" "${GFLAGS_LIB}") 

add_executable(matrix_read
  matrix_read.cpp
  audio_processor.cpp
#  thread_manager.cpp
)

set_property(TARGET matrix_read PROPERTY CXX_STANDARD 11)

target_link_libraries(matrix_read matrix_creator_hal)
target_link_libraries(matrix_read ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(matrix_read ${WIRINGPI_LIB} ${WIRINGPI_DEV_LIB} ${CRYPT_LIB})
target_link_libraries(matrix_read ${GFLAGS_LIB})
target_link_libraries(matrix_read paho-mqttpp3 paho-mqtt3as)
