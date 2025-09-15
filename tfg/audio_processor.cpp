// FILE    : audio_processor.cpp
// Autor   : Julio Albisua
// INFO    : En este código se procesa el audio de los micrófonos de la rpi
//           Se retrasmite el audio por mqtt en el canal audio/beanformed
//           y se enciende el led más cercano a la DOA calculada

#include "audio_processor.hpp"
#include "queue.hpp"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <locale>
#include <string>
#include <thread>
#include <vector>
#include <atomic>

constexpr auto WAV_HEADER_LEN = 44L;

inline void ltrim_string(std::string &s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch)
                                    { return !std::isspace(ch); }));
}

// Trim string from the end (in place)
inline void rtrim_string(std::string &s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch)
                         { return !std::isspace(ch); })
                .base(),
            s.end());
}

// Prototipo WAV
void write_wav_header(
    std::ofstream &out,
    uint32_t sample_rate,
    uint16_t bits_per_sample,
    uint16_t num_channels,
    uint32_t data_size_bytes)
{
    uint32_t byte_rate = sample_rate * num_channels * bits_per_sample / 8;
    uint16_t block_align = num_channels * bits_per_sample / 8;
    uint32_t size_with_header = 36 + data_size_bytes;

    auto old_pos = out.tellp();

    out.seekp(0, std::ios::beg);
    out.write("RIFF", 4);
    out.write(reinterpret_cast<const char *>(&size_with_header), 4);
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
    out.write(reinterpret_cast<const char *>(&data_size_bytes), 4);

    if (old_pos != 0 && old_pos != -1) {
      // We sometimes need to update the wav header (the lenght can change),
      // so if we weren't at the start of the file, restore the old position
      out.seekp(old_pos);
    }
}

// Captura multicanal
void capture_audio(
    matrix_hal::MicrophoneArray *mic_array,
    SafeQueue<AudioBlock> &queue,
    std::atomic_bool& running,
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

void record_all_channels_wav(
    SafeQueue<AudioBlock> &queue,
    matrix_hal::MicrophoneArray *mic_array,
    std::atomic_bool& running,
    int duration,
    std::string folder,
    std::string initial_wav_filename)
{
    const uint32_t SAMPLES_PER_BLOCK = mic_array->NumberOfSamples();
    // const uint16_t NUM_CHANNELS = mic_array->Channels();
    constexpr uint16_t NUM_CHANNELS_ = 8; // Hardcoded because we need constexpr
    const size_t BITS_PER_SAMPLE = 16;
    const size_t BITS_PER_BYTE = 2;
    const uint32_t frequency = mic_array->SamplingRate();

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

    std::array<std::string, NUM_CHANNELS_> filenames;
    std::array<std::ofstream, NUM_CHANNELS_> filehandles;

    for (size_t i = 0; i < NUM_CHANNELS_; i++)
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
        // TODO: We could use wait_pop and avoid this monstruosity.
        if (!queue.pop(block))
        {
            using namespace std::literals::chrono_literals;
            double f = frequency / 1.0;
            // Time to read 512 samples, and avoid wakeups during the blocking of the reading thread
            auto time = ((1.0 / f) * SAMPLES_PER_BLOCK);
            auto s_ms = (1ms * time * 1000.0); // *10e3 for converting s to ms.
            std::this_thread::sleep_for(s_ms);
            continue;
        }

        uint32_t block_size = block.samples[0].size();
        std::vector<std::vector<int16_t>> audios(NUM_CHANNELS_, std::vector<int16_t>(block_size, 0));

        for (size_t i = 0; i < NUM_CHANNELS_; i++)
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

std::vector<std::vector<int16_t>> capture_audio_sync(matrix_hal::MicrophoneArray *mic_array) {
    const uint32_t BLOCK_SIZE = mic_array->NumberOfSamples();
    const uint16_t CHANNELS = mic_array->Channels();

    mic_array->Read();
    AudioBlock block;
    block.samples.resize(CHANNELS, std::vector<int16_t>(BLOCK_SIZE));
    for (uint32_t s = 0; s < BLOCK_SIZE; ++s) {
        for (uint16_t ch = 0; ch < CHANNELS; ++ch) {
            block.samples[ch][s] = mic_array->At(s, ch);
        }
    }

    return block.samples;
}

void record_all_channels_wav_sync(matrix_hal::MicrophoneArray *mic_array,
                                  AudioBlock data,
                                  std::string filename_without_extension) {
  const uint32_t frequency = mic_array->SamplingRate();
  const uint32_t BITS_PER_SAMPLE = 16;
  const uint32_t WAV_CHANNELS = 1;
  constexpr uint16_t NUM_CHANNELS_ = 8; // Hardcoded because we need constexpr
  // const uint16_t NUM_CHANNELS = mic_array->Channels();

  if (filename_without_extension.empty()) {
    filename_without_extension = "output";
  }

  // Trim spaces in the filenames strings
  rtrim_string(filename_without_extension);
  ltrim_string(filename_without_extension);

  std::array<std::string, NUM_CHANNELS_> filenames;
  std::array<std::ofstream, NUM_CHANNELS_> filehandles_out;
  std::array<uint32_t, NUM_CHANNELS_> initial_size_with_header{0, 0, 0, 0,
                                                               0, 0, 0, 0};
  std::array<std::vector<char>, NUM_CHANNELS_> initial_data{};

  for (size_t i = 0; i < NUM_CHANNELS_; i++) {
    std::string wavname =
        filename_without_extension + "_ch_" + std::to_string(i + 1) + ".wav";
    filenames[i] = wavname;
    initial_data[i] = std::vector<char>{};

    if (std::filesystem::exists(wavname)) {
      // We want to append to an already existing file.
      // From experimenting the best way is to read all the files into a
      // vector, and the write all the data into the file again, rewritting
      // the header with the new lenght (the old one + the one of the new
      // data) before writting the new data.

      std::ifstream filehandle(wavname, std::ios::binary);
      // Go to the end and get the position for obtaining the length.
      filehandle.seekg(0, std::ios::end);
      initial_size_with_header[i] = filehandle.tellg();

      // Go back to the beggining of the file
      filehandle.seekg(0, std::ios::beg);

      initial_data[i].reserve(initial_size_with_header[i]);
      initial_data[i].assign(std::istreambuf_iterator<char>(filehandle),
                             std::istreambuf_iterator<char>());
    }

    filehandles_out[i] = std::ofstream(wavname, std::ios::binary);
    if (!filehandles_out[i].is_open()) {
      std::cerr << "Error abriendo " << wavname << "para grabar" << std::endl;
      return;
    }
  }

  for (size_t i = 0; i < NUM_CHANNELS_; i++) {

    filehandles_out[i].write(
        reinterpret_cast<const char *>(initial_data[i].data()),
        initial_data[i].size() * sizeof(char));

    std::vector<int16_t> new_audio{data.samples[i]};
    uint32_t new_audio_len = new_audio.size() * sizeof(int16_t);
    write_wav_header(
        filehandles_out[i], frequency, BITS_PER_SAMPLE, WAV_CHANNELS,
        new_audio_len + initial_size_with_header[i] - WAV_HEADER_LEN);
    filehandles_out[i].write(reinterpret_cast<const char *>(new_audio.data()),
                             new_audio_len);
  }
}
