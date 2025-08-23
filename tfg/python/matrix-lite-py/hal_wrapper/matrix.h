#ifndef MATRIX_H
#define MATRIX_H

#include "../../../../cpp/driver/matrixio_bus.h"
#include <map>
#include <pybind11/pybind11.h>

namespace py = pybind11;

// Global object for MATRIX hardware communication
extern matrix_hal::MatrixIOBus bus;

// Helpful functions for pybind11
namespace pyHelp {
std::string to_lower_case(py::str);
std::map<std::string, pybind11::handle> dict_to_map(py::dict);
} // namespace pyHelp
#endif
