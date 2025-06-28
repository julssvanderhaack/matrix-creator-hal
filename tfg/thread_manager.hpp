#ifndef THREAD_MANAGER_HPP
#define THREAD_MANAGER_HPP

#include <thread>
#include <vector>
#include <fstream>
#include "matrix_hal/microphone_array.h"
#include "utils.hpp"

void start_threads(matrix_hal::MicrophoneArray* mic_array,
                   SafeQueue<AudioBlock>& queue,
                   std::vector<std::ofstream>& data_to_file,
                   int frequency,
                   int duration,
                   int processor_threads = 3);

#endif

