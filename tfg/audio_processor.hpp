#ifndef AUDIO_PROCESSOR_HPP
#define AUDIO_PROCESSOR_HPP

#include <vector>
#include <atomic>
#include "matrix_hal/microphone_array.h"
#include "mqtt/async_client.h"
#include "utils.hpp"

// Variables globales para control de ejecución y MQTT
extern std::atomic<bool> running;
extern mqtt::async_client client;
extern const std::string BEAMFORMED_TOPIC;

// Prototipos de funciones principales
void capture_audio(matrix_hal::MicrophoneArray* mic_array,
                   SafeQueue<AudioBlock>& queue,
                   int duration);

void process_beamforming(SafeQueue<AudioBlock>& queue,
                         uint32_t frequency,
                         int duration);

#endif