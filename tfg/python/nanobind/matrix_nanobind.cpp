#include "matrix_nanobind.hpp"

#include "../../../cpp/driver/microphone_array.h"
#include <ostream>
#include <iostream>

#include <nanobind/nanobind.h>

int add(int a, int b)
{
    // Inicializar bus MATRIX
    matrix_hal::MatrixIOBus bus;
    if (!bus.Init())
    {
        std::cerr << "Couldn't find bus" << std::endl;
    }

    std::cout << "Sum from nanobind" << std::endl;
    return a + b;
}

NB_MODULE(matrix_nanobind, m)
{
    m.def("add", &add);
}
