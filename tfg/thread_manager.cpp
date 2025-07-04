#include <thread>
#include <atomic>
#include <iostream>
#include "thread_manager.hpp"
#include "matrix_hal/microphone_array.h"
//#include "safe_queue.hpp"
#include "audio_processor.hpp"

extern std::atomic<bool> running;

void start_threads(matrix_hal::MicrophoneArray* mic_array,
                   SafeQueue<AudioBlock>& queue,
                   std::vector<std::ofstream>& data_to_file,
                   uint32_t frequency,
                   int duration) {
    const uint16_t CHANNELS = mic_array->Channels();

    std::thread capture_thread(capture_audio, mic_array, std::ref(queue), frequency, duration);

    std::vector<std::thread> processing_threads;
    for (int i = 0; i < 3; ++i) {
        processing_threads.emplace_back(process_audio, std::ref(queue), std::ref(data_to_file), CHANNELS);
    }

    capture_thread.join();
    running = false;  // Señal de parada a los procesadores

    for (auto& t : processing_threads) t.join();
}