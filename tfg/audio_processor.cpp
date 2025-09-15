// FILE    : audio_processor.cpp
// Autor   : Julio Albisua
// INFO    : En este código se procesa el audio de los micrófonos de la rpi
//           Se retrasmite el audio por mqtt en el canal audio/beanformed
//           y se enciende el led más cercano a la DOA calculada

#include "audio_processor.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <fstream>

// Prototipo WAV
void write_wav_header(
    std::ofstream &out,
    uint32_t sample_rate,
    uint16_t bits_per_sample,
    uint16_t num_channels,
    uint32_t data_size)
{
    uint32_t byte_rate = sample_rate * num_channels * bits_per_sample / 8;
    uint16_t block_align = num_channels * bits_per_sample / 8;
    uint32_t chunk_size = 36 + data_size;

    out.seekp(0, std::ios::beg);
    out.write("RIFF", 4);
    out.write(reinterpret_cast<const char *>(&chunk_size), 4);
    out.write("WAVE", 4);
    out.write("fmt ", 4);

    uint32_t subchunk1_size = 16;
    uint16_t audio_format = 1; // PCM
    out.write(reinterpret_cast<const char *>(&subchunk1_size), 4);
    out.write(reinterpret_cast<const char *>(&audio_format), 2);
    out.write(reinterpret_cast<const char *>(&num_channels), 2);
    out.write(reinterpret_cast<const char *>(&sample_rate), 4);
    out.write(reinterpret_cast<const char *>(&byte_rate), 4);
    out.write(reinterpret_cast<const char *>(&block_align), 2);
    out.write(reinterpret_cast<const char *>(&bits_per_sample), 2);
    out.write("data", 4);
    out.write(reinterpret_cast<const char *>(&data_size), 4);
}

// Captura multicanal
void capture_audio(
    matrix_hal::MicrophoneArray *mic_array,
    SafeQueue<AudioBlock> &queue,
    int duration)
{
    const uint32_t BLOCK_SIZE = mic_array->NumberOfSamples();
    const uint16_t CHANNELS = mic_array->Channels();
    auto end_time = std::chrono::steady_clock::now() + std::chrono::seconds(duration);

    while (running && (duration == 0 || std::chrono::steady_clock::now() < end_time))
    {
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
