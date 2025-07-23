// audio_processor.cpp
#include "audio_processor.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <fstream>
#include <cmath>

#define SPEED_OF_SOUND 343.0f      // Velocidad del sonido (m/s)
#define MIC_DISTANCE    0.04f      // Distancia entre micrófonos adyacentes (m)

float normalize_angle(float angle_deg) {
    while (angle_deg > 180.0f)  angle_deg -= 360.0f;
    while (angle_deg < -180.0f) angle_deg += 360.0f;
    return angle_deg;
}

// Prototipo WAV
static void write_wav_header(
    std::ofstream &out,
    uint32_t sample_rate,
    uint16_t bits_per_sample,
    uint16_t num_channels,
    uint32_t data_size
) {
    uint32_t byte_rate   = sample_rate * num_channels * bits_per_sample / 8;
    uint16_t block_align = num_channels * bits_per_sample / 8;
    uint32_t chunk_size  = 36 + data_size;

    out.seekp(0, std::ios::beg);
    out.write("RIFF", 4);
    out.write(reinterpret_cast<const char*>(&chunk_size), 4);
    out.write("WAVE", 4);
    out.write("fmt ", 4);

    uint32_t subchunk1_size = 16;
    uint16_t audio_format   = 1; // PCM
    out.write(reinterpret_cast<const char*>(&subchunk1_size), 4);
    out.write(reinterpret_cast<const char*>(&audio_format), 2);
    out.write(reinterpret_cast<const char*>(&num_channels), 2);
    out.write(reinterpret_cast<const char*>(&sample_rate), 4);
    out.write(reinterpret_cast<const char*>(&byte_rate), 4);
    out.write(reinterpret_cast<const char*>(&block_align), 2);
    out.write(reinterpret_cast<const char*>(&bits_per_sample), 2);
    out.write("data", 4);
    out.write(reinterpret_cast<const char*>(&data_size), 4);
}

// Captura multicanal
void capture_audio(
    matrix_hal::MicrophoneArray* mic_array,
    SafeQueue<AudioBlock>& queue,
    int duration
) {
    const uint32_t BLOCK_SIZE = mic_array->NumberOfSamples();
    const uint16_t CHANNELS   = mic_array->Channels();
    auto end_time = std::chrono::steady_clock::now() + std::chrono::seconds(duration);

    while (running && (duration == 0 || std::chrono::steady_clock::now() < end_time)) {
        mic_array->Read();
        AudioBlock block;
        block.samples.resize(CHANNELS, std::vector<int16_t>(BLOCK_SIZE));
        for (uint32_t s = 0; s < BLOCK_SIZE; ++s)
            for (uint16_t ch = 0; ch < CHANNELS; ++ch)
                block.samples[ch][s] = mic_array->At(s, ch);
        queue.push(block);
    }
    running = false;
}

// Delay-and-Sum con barrido de ángulos + Everloop
void process_beamforming(
    SafeQueue<AudioBlock>& queue,
    uint32_t frequency,
    int duration,
    matrix_hal::Everloop* everloop,
    matrix_hal::EverloopImage* image
) {
    const uint16_t num_channels   = 8;
    const uint16_t bits_per_sample= 16;
    uint32_t estimated_samples    = frequency * duration;
    uint32_t data_size            = estimated_samples * bits_per_sample / 8;

    std::ofstream outfile("beamformed_output.wav", std::ios::binary);
    if (!outfile.is_open()) {
        std::cerr << "Error abriendo beamformed_output.wav\n";
        running = false;
        return;
    }
    write_wav_header(outfile, frequency, bits_per_sample, 1, data_size);

    const float RADIUS   	 = MIC_DISTANCE / (2.0f * sinf(M_PI / num_channels));
    const float ANGLE_MIN 	 = -180.0f, ANGLE_MAX = 180.0f, ANGLE_STEP = 5.0f;       
    const int   num_leds  = image->leds.size();

    while (running) {
        AudioBlock block;
        if (!queue.pop(block)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        uint32_t block_size = block.samples[0].size();
        float max_energy = -1.0f, best_angle = 0.0f;
        std::vector<int16_t> best_output(block_size);

        for (float angle_deg = ANGLE_MIN; angle_deg <= ANGLE_MAX; angle_deg += ANGLE_STEP) {
            float doa_rad = angle_deg * M_PI / 180.0f;
            std::vector<int32_t> sum(block_size, 0);

            for (uint16_t ch = 0; ch < num_channels; ++ch) {
                float mic_angle = 2.0f * M_PI * ch / num_channels;
                float x = RADIUS * cosf(mic_angle);
                float y = RADIUS * sinf(mic_angle);
                float delay_sec = (x * cosf(doa_rad) + y * sinf(doa_rad)) / SPEED_OF_SOUND;
                int delay_samples = static_cast<int>(round(delay_sec * frequency));

                for (uint32_t i = 0; i < block_size; ++i) {
                    int idx = static_cast<int>(i) + delay_samples;
                    if (idx >= 0 && idx < static_cast<int>(block_size))
                        sum[i] += block.samples[ch][idx];
                }
            }

            std::vector<int16_t> beamformed(block_size);
            float energy = 0.0f;
            for (uint32_t i = 0; i < block_size; ++i) {
                beamformed[i] = sum[i] / num_channels;
                energy += beamformed[i] * beamformed[i];
            }
            if (energy > max_energy) {
                max_energy = energy;
                best_output = beamformed;
                best_angle  = angle_deg;
            }
        }

        float ANGLE_CORRECTION = 15.0f;
        std::cout << "DOA Calculada: " << normalize_angle(best_angle - ANGLE_CORRECTION) << " grados\n";

        // ——— Everloop: limpia, calcula LED y enciende —
        for (auto &led : image->leds) {
            led.red = led.green = led.blue = 0;
        }

        float LED_CORRECTION = -110.0f;
        float angle01 = (normalize_angle(best_angle + LED_CORRECTION)  + 180.0f) / 360.0f ;  
        int pin = static_cast<int>(round(angle01 * (num_leds - 1)));
        image->leds[pin].green = 30;

        everloop->Write(image);

        // ——— Guarda WAV y publica MQTT —
        outfile.write(
            reinterpret_cast<const char*>(best_output.data()),
            best_output.size() * sizeof(int16_t)
        );


    }

    outfile.close();
}
