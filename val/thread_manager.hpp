#ifndef THREAD_MANAGER_HPP
#define THREAD_MANAGER_HPP

#include <vector>
#include <fstream>
#include "matrix_hal/microphone_array.h"
//#include "safe_queue.hpp"
#include "audio_processor.hpp"

void start_threads(matrix_hal::MicrophoneArray* mic_array,
                   SafeQueue<AudioBlock>& queue,
                   std::vector<std::ofstream>& data_to_file,
                   uint32_t frequency,
                   int duration);

#endif  // THREAD_MANAGER_HPP