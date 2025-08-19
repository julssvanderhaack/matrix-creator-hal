#include "matrix_python.hpp"

#include "../../../cpp/driver/microphone_array.h"
#include <ostream>
#include <iostream>

#include <pybind11/pybind11.h>

int add(int i, int j)
{
    // Inicializar bus MATRIX
    matrix_hal::MatrixIOBus bus;
    if (!bus.Init())
    {
        std::cerr << "Couldn't find bus" << std::endl;
    }


    std::cout << "Sum from pybind" << std::endl;
    return i + j;
}

PYBIND11_MODULE(matrix_pybind, m)
{
    m.doc() = "pybind11 example plugin"; // optional module docstring

    m.def("add", &add, "A function that adds two numbers");
}