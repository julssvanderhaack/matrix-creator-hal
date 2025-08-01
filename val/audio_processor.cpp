// audio_processor.cpp
#include "audio_processor.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <fstream>
#include <cmath>
#include <array>
using namespace std::literals::chrono_literals;

#define SPEED_OF_SOUND 343.0f   // Velocidad del sonido (m/s)
#define MIC_DISTANCE 0.04f      // Distancia entre micrófonos adyacentes (m)
#define NUM_CHANNELS 8          // Number of microphones in the matrix device
#define BITS_PER_SAMPLE 16      // Bits per sample of the recorded audio
#define BITS_PER_BYTE 8         // Each byte has 8 bits
#define SAMPLES_PER_BLOCK 512.0 // Each block has 512 samples, and the adquisition thread is blocked otherwise

// Prototipo WAV
// TODO: This function is wrong the matlab and android players don't like it
static void write_wav_header(
    std::ofstream &out,
    uint32_t sample_rate,
    uint16_t bits_per_sample,
    uint16_t num_channels_wav,
    uint32_t data_size)
{
    uint32_t byte_rate = sample_rate * num_channels_wav * BITS_PER_SAMPLE / BITS_PER_BYTE;
    uint16_t block_align = num_channels_wav * BITS_PER_SAMPLE / BITS_PER_BYTE;
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

// Recordings
void record_all_channels_wav(
    SafeQueue<AudioBlock> &queue,
    uint32_t frequency,
    int duration,
    std::string folder,
    std::string initial_wav_filename)
{
    uint32_t estimated_samples = frequency * duration;
    uint32_t data_size = estimated_samples * BITS_PER_SAMPLE / BITS_PER_BYTE;

    if (initial_wav_filename.empty())
    {
        initial_wav_filename = "output";
    }

    // Trim spaces in the filenames strings
    rtrim_string(initial_wav_filename);
    ltrim_string(initial_wav_filename);

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

    std::string desired_extension = ".wav";
    std::string maybe_extension = std::string("");
    if (initial_wav_filename.length() < 4)
    {
        maybe_extension = desired_extension;
    }
    else
    {
        auto idx = initial_wav_filename.rfind('.');
        if (idx != std::string::npos)
        {
            // There is a . in the filename, check if the extension is .wav (rfind finds the last .)
            std::string file_extension = initial_wav_filename.substr(idx + 1);
            if (file_extension != desired_extension)
            {
                // The file has other extension
                maybe_extension = desired_extension;
            }
            else
            {
                // The file has .wav as an extension
                maybe_extension = std::string("");
            }
        }
        else // There is no extension
        {
            maybe_extension = desired_extension;
        }
    }

    // Default to current folder
    if (folder.empty())
    {
        folder = ".";
    }
    else
    { // Remove last / of folder, we add it ourselfs
        auto last_character_folder = folder.back();
        if (last_character_folder == '/')
        {
            folder.pop_back();
        }
    }

    std::array<std::string, NUM_CHANNELS> filenames;
    std::array<std::ofstream, NUM_CHANNELS> filehandles;

    for (size_t i = 0; i < NUM_CHANNELS; i++)
    {
        std::string wavname = folder + "/" + "ch" + std::to_string(i + 1) + "-" + time_str + "-" + initial_wav_filename + maybe_extension;
        filenames[i] = wavname;
        filehandles[i] = std::move(std::ofstream(wavname, std::ios::binary)); // Is the move needed?
        if (!filehandles[i].is_open())
        {
            std::cerr << "Error abriendo " << wavname << "para grabar" << std::endl;
            running = false;
            return;
        }
        write_wav_header(filehandles[i], frequency, BITS_PER_SAMPLE, 1, data_size);
    }

    while (running)
    {
        AudioBlock block;
        if (!queue.pop(block))
        {
            // std::this_thread::sleep_for(std::chrono::milliseconds(1));
            double f = frequency / 1.0;
            auto time = ((1.0 / f) * SAMPLES_PER_BLOCK * 1000.0);
            auto s_ms = (1ms * time);
            // std::cerr << "Sleeping for " << time << std::endl;
            std::this_thread::sleep_for(s_ms);
            continue;
        }

        uint32_t block_size = block.samples[0].size();
        std::vector<std::vector<int16_t>> audios(NUM_CHANNELS, std::vector<int16_t>(block_size, 0));

        for (size_t i = 0; i < NUM_CHANNELS; i++)
        {
            std::vector<int16_t> ch_audio(block.samples[i]);
            // Write inside the while(running) loop, that way we write all the
            // data as soon as we can take it. Also Write automatically advances the
            // handle
            filehandles[i].write(
                reinterpret_cast<const char *>(ch_audio.data()),
                ch_audio.size() * sizeof(int16_t));
        }
    }
}

void record_all_channels_raw(
    SafeQueue<AudioBlock> &queue,
    uint32_t frequency,
    int duration,
    std::string folder,
    std::string initial_raw_filename)
{
    uint32_t estimated_samples = frequency * duration;
    uint32_t data_size = estimated_samples * BITS_PER_SAMPLE / BITS_PER_BYTE;
    data_size = data_size; // Avoid build warnings, we want the same interface that with wav

    if (initial_raw_filename.empty())
    {
        initial_raw_filename = "output";
    }

    // Trim spaces in the filenames strings
    rtrim_string(initial_raw_filename);
    ltrim_string(initial_raw_filename);

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

    std::string desired_extension = ".raw";
    std::string maybe_extension = std::string("");
    if (initial_raw_filename.length() < 4)
    {
        maybe_extension = desired_extension;
    }
    else
    {
        auto idx = initial_raw_filename.rfind('.');
        if (idx != std::string::npos)
        {
            // There is a . in the filename, check if the extension is .wav (rfind finds the last .)
            std::string file_extension = initial_raw_filename.substr(idx + 1);
            if (file_extension != desired_extension)
            {
                // The file has other extension
                maybe_extension = desired_extension;
            }
            else
            {
                // The file has .wav as an extension
                maybe_extension = std::string("");
            }
        }
        else // There is no extension
        {
            maybe_extension = desired_extension;
        }
    }

    if (folder.empty())
    { // Default to current folder
        folder = ".";
    }
    else
    { // Remove last / from folder name, we add it ourselfs
        auto last_character_folder = folder.back();
        if (last_character_folder == '/')
        {
            folder.pop_back();
        }
    }

    std::array<std::string, NUM_CHANNELS> filenames;
    std::array<std::ofstream, NUM_CHANNELS> filehandles;

    for (size_t i = 0; i < NUM_CHANNELS; i++)
    {
        std::string raw_file_name = folder + "/" + "ch" + std::to_string(i + 1) + "-" + time_str + "-" + initial_raw_filename + maybe_extension;
        filenames[i] = raw_file_name;
        filehandles[i] = std::move(std::ofstream(raw_file_name, std::ios::binary)); // Is the move needed?
        if (!filehandles[i].is_open())
        {
            std::cerr << "Error abriendo " << raw_file_name << "para grabar" << std::endl;
            running = false;
            return;
        }
    }

    while (running)
    {
        AudioBlock block;
        if (!queue.pop(block))
        {
            // std::this_thread::sleep_for(std::chrono::milliseconds(1));
            double f = frequency / 1.0;
            double time = ((1.0 / f) * SAMPLES_PER_BLOCK * 1000.0);
            auto s_ms = (1ms * time);
            // std::cerr << "Sleeping for " << time << std::endl;
            std::this_thread::sleep_for(s_ms);
            continue;
        }

        uint32_t block_size = block.samples[0].size();
        std::vector<std::vector<int16_t>> audios(NUM_CHANNELS, std::vector<int16_t>(block_size, 0));

        for (size_t i = 0; i < NUM_CHANNELS; i++)
        {
            std::vector<int16_t> ch_audio(block.samples[i]);
            // Write inside the while(running) loop, that way we write all the
            // data as soon as we can take it. Also Write automatically advances the
            // handle
            filehandles[i].write(
                reinterpret_cast<const char *>(ch_audio.data()),
                ch_audio.size() * sizeof(int16_t));
        }
    }
}
