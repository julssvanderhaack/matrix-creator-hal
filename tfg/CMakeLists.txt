project(matrix_read C CXX)
cmake_minimum_required(VERSION 3.5)

include (../cmake/FatalWarnings.cmake)
ADM_EXTRA_WARNINGS()

add_subdirectory(../cpp/driver driver)

find_package(Threads REQUIRED)
find_library (WIRINGPI_LIB     NAMES wiringPi)
find_library (WIRINGPI_DEV_LIB NAMES wiringPiDev)
find_library (CRYPT_LIB        NAMES crypt)
find_library (GFLAGS_LIB       NAMES gflags)
message(STATUS "gflags found => ${GFLAGS_LIB}")

# ------------------ Ejecutable C++ (igual que ya tenías) ------------------
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

# ------------------ Módulo Python con pybind11 ------------------
if(BUILD_PYTHON)
  # Necesita cabeceras y libs de Python y pybind11 (paquetes dev)
  find_package(Python3 COMPONENTS Interpreter Development.Module REQUIRED)
  find_package(pybind11 CONFIG QUIET)

  if(NOT pybind11_FOUND)
    message(STATUS "pybind11 no está en el sistema; usando subdirectorio vendorizado (third_party/pybind11).")
    # Si tienes pybind11 como submódulo, descomenta y ajusta ruta:
    # add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../third_party/pybind11 pybind11_build)
  endif()

  pybind11_add_module(matrix_hal_tfg
    MODULE
    matrix_read_py.cpp
    audio_processor.cpp
  )

  set_property(TARGET matrix_hal_tfg PROPERTY CXX_STANDARD 17)
  set_target_properties(matrix_hal_tfg PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tfg"
  )

  target_link_libraries(matrix_hal_tfg PRIVATE
    matrix_creator_hal
    ${CMAKE_THREAD_LIBS_INIT}
    ${WIRINGPI_LIB} ${WIRINGPI_DEV_LIB} ${CRYPT_LIB}
    paho-mqttpp3 paho-mqtt3as
    # gflags no es imprescindible en el módulo; inclúyelo si lo necesitas:
    ${GFLAGS_LIB}
  )

  # RPATH para que encuentre dependencias locales al importar
  set_target_properties(matrix_hal_tfg PROPERTIES
    BUILD_RPATH "\$ORIGIN:\$ORIGIN/driver"
    INSTALL_RPATH "\$ORIGIN:\$ORIGIN/driver"
  )
endif()
