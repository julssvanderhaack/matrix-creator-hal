#pragma once

#include <vector>
#include <atomic>
#include "../cpp/driver/microphone_array.h"
#include "../cpp/driver/everloop.h"
#include "../cpp/driver/everloop_image.h"
#include "mqtt/async_client.h"
#include "utils.hpp"

// Variables globales
extern std::atomic<bool> running;
extern mqtt::async_client client;
extern const std::string BEAMFORMED_TOPIC;

// Prototipos
void capture_audio(
    matrix_hal::MicrophoneArray *mic_array,
    SafeQueue<AudioBlock> &queue,
    int duration);

void record_all_channels_wav(
    SafeQueue<AudioBlock> &queue,
    uint32_t frequency,
    int duration,
    std::string initial_wav_filename);