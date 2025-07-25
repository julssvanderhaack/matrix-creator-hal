// audio_processor.cpp
#include "audio_processor.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <fstream>
#include <cmath>
#include <array>

#define SPEED_OF_SOUND 343.0f // Velocidad del sonido (m/s)
#define MIC_DISTANCE 0.04f    // Distancia entre micrófonos adyacentes (m)
#define NUM_CHANNELS 8

float normalize_angle(float angle_deg)
{
    while (angle_deg > 180.0f)
        angle_deg -= 360.0f;
    while (angle_deg < -180.0f)
        angle_deg += 360.0f;
    return angle_deg;
}

// Prototipo WAV
static void write_wav_header(
    std::ofstream &out,
    uint32_t sample_rate,
    uint16_t bits_per_sample,
    uint16_t num_channels_wav,
    uint32_t data_size)
{
    uint32_t byte_rate = sample_rate * num_channels_wav * bits_per_sample / 8;
    uint16_t block_align = num_channels_wav * bits_per_sample / 8;
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
    out.write(reinterpret_cast<const char *>(&num_channels_wav), 2);
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

// Delay-and-Sum con barrido de ángulos + Everloop
void record_and_beamforming(
    SafeQueue<AudioBlock> &queue,
    uint32_t frequency,
    int duration,
    matrix_hal::Everloop *everloop,
    matrix_hal::EverloopImage *image,
    std::string initial_wav_filename)
{
    const uint16_t num_channels = 8;
    const uint16_t bits_per_sample = 16;
    uint32_t estimated_samples = frequency * duration;
    uint32_t data_size = estimated_samples * bits_per_sample / 8;

    if (initial_wav_filename.empty())
    {
        initial_wav_filename = "output.wav";
    }

    auto time_str = std::string();
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    char time_buffer[1024]; // This should be enough for a date no?
                            // "%F-%H-%M-%S". Write %F: iso data yy-mm-dd %H:hour, %M: minute, %S: second
    if (std::strftime(time_buffer, sizeof(time_buffer), "%F-%Hh-%Mm-%Ss", std::localtime(&now_time)))
    {
        time_str = std::string{time_buffer};
    }
    else
    {
        time_str = std::string{"00-00-00-00-00"};
        std::cerr << "Error leyendo fecha" << std::endl;
    }

    std::array<std::string, NUM_CHANNELS> filenames;
    std::array<std::ofstream, NUM_CHANNELS> filehandles;

    for (size_t i = 0; i < NUM_CHANNELS; i++)
    {
        std::string wavname = "ch" + std::to_string(i) + "_" + time_str + initial_wav_filename;
        filenames[i] = wavname;
        filehandles[i] = std::move(std::ofstream(wavname, std::ios::binary)); // Is the move needed?
        if (!filehandles[i].is_open())
        {
            std::cerr << "Error abriendo " << wavname << "para grabar" << std::endl;
            running = false;
            return;
        }
        write_wav_header(filehandles[i], frequency, bits_per_sample, 1, data_size);
    }

    while (running)
    {
        AudioBlock block;
        if (!queue.pop(block))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        uint32_t block_size = block.samples[0].size();
        std::vector<std::vector<int16_t>> audios(NUM_CHANNELS, std::vector<int16_t>(block_size, 0));

        // TODO: I think we don't need this anymore
        // for (uint16_t ch = 0; ch < num_channels; ++ch)
        // {
        //     for (uint32_t i = 0; i < block_size; ++i)
        //     {
        //         if (i >= 0 && i < block_size)
        //             audios[ch][i] = block.samples[ch][i]; // TODO: Don't sum, we want all the info in the channels
        //     }
        // }
        // std::vector<int16_t> ch_audio(audios[i]);

        for (size_t i = 0; i < NUM_CHANNELS; i++)
        {
            std::vector<int16_t> ch_audio(block.samples[i]);
            // Write in>side the while(running) loop, that way we write all the data as soon as we can take it.
            // Write automatically advances the handle
            filehandles[i].write(
                reinterpret_cast<const char *>(ch_audio.data()),
                ch_audio.size() * sizeof(int16_t));
        }
    }

    for (size_t i = 0; i < NUM_CHANNELS; i++)
    {
        // In theory this isn't needed because the ofstream uses RAII and close the file handle automatically
        // According to the docs: Note that any open file is automatically closed when the ofstream object is destroyed.
        filehandles[i].close();
    }
}
