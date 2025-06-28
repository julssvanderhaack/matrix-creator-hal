#ifndef AUDIO_PROCESSOR_HPP
#define AUDIO_PROCESSOR_HPP

#include <vector>
#include <fstream>
#include <atomic>
#include "matrix_hal/microphone_array.h"
#include "mqtt/async_client.h"
#include "utils.hpp"

extern std::atomic<bool> running;
extern mqtt::async_client client;
extern const std::string TOPIC_BASE;

void capture_audio(matrix_hal::MicrophoneArray* mic_array,
                   SafeQueue<AudioBlock>& queue,
                   uint32_t frequency,
                   int duration);

void process_audio(SafeQueue<AudioBlock>& queue,
                   std::vector<std::ofstream>& data_to_file,
                   uint16_t CHANNELS);

#endif

