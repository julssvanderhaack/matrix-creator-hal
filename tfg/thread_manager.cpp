#include "thread_manager.hpp"
#include "audio_processor.hpp"

void start_threads(matrix_hal::MicrophoneArray* mic_array,
                   SafeQueue<AudioBlock>& queue,
                   std::vector<std::ofstream>& data_to_file,
                   int frequency,
                   int duration,
                   int processor_threads) {
    std::thread capture_thread(capture_audio, mic_array, std::ref(queue), frequency, duration);
    std::vector<std::thread> processing_threads;

    for (int i = 0; i < processor_threads; ++i)
        processing_threads.emplace_back(process_audio, std::ref(queue), std::ref(data_to_file), mic_array->Channels());

    capture_thread.join();
    running = false;

    for (auto& t : processing_threads) t.join();
}

