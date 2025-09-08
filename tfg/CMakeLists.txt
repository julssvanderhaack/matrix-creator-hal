project(matrix_read C CXX)
cmake_minimum_required(VERSION 3.5)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include (../cmake/FatalWarnings.cmake)
ADM_EXTRA_WARNINGS()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(../cpp/driver driver)

find_package(Threads REQUIRED)
find_library (WIRINGPI_LIB     NAMES wiringPi)
find_library (WIRINGPI_DEV_LIB NAMES wiringPiDev)
find_library (CRYPT_LIB        NAMES crypt)
find_library (GFLAGS_LIB       NAMES gflags)
message(STATUS "gflags found => ${GFLAGS_LIB}")

add_executable(matrix_read
  matrix_read.cpp
  audio_processor.cpp
)
set_property(TARGET matrix_read PROPERTY CXX_STANDARD 17)

target_link_libraries(matrix_read PRIVATE
  matrix_creator_hal
  ${CMAKE_THREAD_LIBS_INIT}
  ${WIRINGPI_LIB} ${WIRINGPI_DEV_LIB} ${CRYPT_LIB}
  ${GFLAGS_LIB}
  paho-mqttpp3 paho-mqtt3as
)


add_executable(test_record_sync
  test_record_sync.cpp
  audio_processor.cpp
)
set_property(TARGET test_record_sync PROPERTY CXX_STANDARD 17)

target_link_libraries(test_record_sync PRIVATE
  matrix_creator_hal
  ${CMAKE_THREAD_LIBS_INIT}
  ${WIRINGPI_LIB} ${WIRINGPI_DEV_LIB} ${CRYPT_LIB}
  ${GFLAGS_LIB}
  paho-mqttpp3 paho-mqtt3as
)

add_executable(test_record_async
  test_record_async.cpp
  audio_processor.cpp
)
set_property(TARGET test_record_async PROPERTY CXX_STANDARD 17)

target_link_libraries(test_record_async PRIVATE
  matrix_creator_hal
  ${CMAKE_THREAD_LIBS_INIT}
  ${WIRINGPI_LIB} ${WIRINGPI_DEV_LIB} ${CRYPT_LIB}
  ${GFLAGS_LIB}
  paho-mqttpp3 paho-mqtt3as
)

add_executable(test_mqtt_sync
  test_mqtt_sync.cpp
  audio_processor.cpp
)
set_property(TARGET test_mqtt_sync PROPERTY CXX_STANDARD 17)

target_link_libraries(test_mqtt_sync PRIVATE
  matrix_creator_hal
  ${CMAKE_THREAD_LIBS_INIT}
  ${WIRINGPI_LIB} ${WIRINGPI_DEV_LIB} ${CRYPT_LIB}
  ${GFLAGS_LIB}
  paho-mqttpp3 paho-mqtt3as
)

add_executable(test_mqtt_async
  test_mqtt_async.cpp
  audio_processor.cpp
)
set_property(TARGET test_mqtt_async PROPERTY CXX_STANDARD 17)

target_link_libraries(test_mqtt_async PRIVATE
  matrix_creator_hal
  ${CMAKE_THREAD_LIBS_INIT}
  ${WIRINGPI_LIB} ${WIRINGPI_DEV_LIB} ${CRYPT_LIB}
  ${GFLAGS_LIB}
  paho-mqttpp3 paho-mqtt3as
)
