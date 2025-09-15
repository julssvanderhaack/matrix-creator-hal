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
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mqtt/client.h>
#include <string>
#include <thread>
#include <vector>

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

void capture_audio(matrix_hal::MicrophoneArray *mic_array,
                   SafeQueue<AudioBlock> &queue, std::atomic_bool &running) {
  const uint32_t BLOCK_SIZE = mic_array->NumberOfSamples();
  const uint16_t CHANNELS = mic_array->Channels();

  while (running) {
    mic_array->Read();
    AudioBlock block;
    block.samples.resize(CHANNELS, std::vector<int16_t>(BLOCK_SIZE));
    for (uint32_t s = 0; s < BLOCK_SIZE; ++s) {
      for (uint16_t ch = 0; ch < CHANNELS; ++ch) {
        block.samples[ch][s] = mic_array->At(s, ch);
      }
    }
    queue.push(block);
  }
}

void record_all_channels_wav(SafeQueue<AudioBlock> &queue,
                             matrix_hal::MicrophoneArray *mic_array,
                             std::atomic_bool &running,
                             std::string filename_without_extension = "output",
                             bool drain = true) {
  // const uint16_t NUM_CHANNELS = mic_array->Channels();
  constexpr uint16_t NUM_CHANNELS_ = 8; // Hardcoded because we need constexpr
  const uint32_t frequency = mic_array->SamplingRate();
  const uint32_t BITS_PER_SAMPLE = 16;
  const uint32_t WAV_CHANNELS = 1;

  std::array<std::string, NUM_CHANNELS_> filenames;
  std::array<std::ofstream, NUM_CHANNELS_> filehandles;

  for (size_t i = 0; i < NUM_CHANNELS_; i++) {
    std::string wavname =
        filename_without_extension + "_ch_" + std::to_string(i + 1) + ".wav";
    filenames[i] = wavname;

    filehandles[i] = std::ofstream(wavname, std::ios::binary);
    if (!filehandles[i].is_open()) {
      std::cerr << "Error abriendo " << wavname << "para grabar" << std::endl;
      return;
    }
  }

  std::array<uint32_t, NUM_CHANNELS_>  audio_lens = {0,0,0,0,0,0,0,0};

  // Continue processing while running = true, or we want to drain all the
  // data in the queue, which has data. If we don't drain we can leave the
  // queue with data at the end of the process.
  while (running || (drain && !queue.empty())) {
    AudioBlock block;
    // This thread is going to sleep until we have an element in the queue, if
    // it's empty
    int ret_pop = queue.wait_pop(block);
    if (ret_pop == 0) {
        continue; // We are not running and we have no more data in the queue.
    }

    uint32_t block_size = block.samples[0].size();
    std::vector<std::vector<int16_t>> audios(
        NUM_CHANNELS_, std::vector<int16_t>(block_size, 0));

    for (size_t i = 0; i < NUM_CHANNELS_; i++) {
      std::vector<int16_t> ch_audio(block.samples[i]);
      audio_lens[i] = audio_lens[i] + ch_audio.size() * sizeof(int16_t);
      write_wav_header(filehandles[i], frequency, BITS_PER_SAMPLE, WAV_CHANNELS,
                       audio_lens[i]);

      // Write inside the while(running) loop, that way we write all the
      // data as soon as we can take it. Also Write automatically advances the
      // handle
      filehandles[i].write(reinterpret_cast<const char *>(ch_audio.data()),
                           ch_audio.size() * sizeof(int16_t));
    }
  }
}

AudioBlock capture_audio_sync(matrix_hal::MicrophoneArray *mic_array) {
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

    return block;
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

bool connect_sync_mqtt_client(mqtt::client &client,
                              mqtt::connect_options *conn_opts = nullptr) {
  try {
    if (conn_opts == nullptr) {
      mqtt::connect_options connOpts;
      connOpts.set_clean_session(true);
      client.connect(connOpts);
    } else {
      client.connect(*conn_opts);
    }
  } catch (const mqtt::exception &exc) {
    std::cerr << "Connect sync MQTT error" << exc.what() << std::endl;
    return false;
  }
  return true;
}

bool disconnect_sync_mqtt_client(mqtt::client &client) {
  if (client.is_connected()) {
    try {
      client.disconnect();
      return true;
    } catch (const mqtt::exception &exc) {
      std::cerr << "Disconnect sync MQTT error" << exc.what() << std::endl;
      return false;
    }
  } else {
    std::cerr << "Client already disconnected" << std::endl;
    return false;
  }
}

void send_audio_mqtt_sync(mqtt::client &client,
                    matrix_hal::MicrophoneArray *mic_array, AudioBlock block,
                    std::string topic_name, int qos = 1,
                    bool send_bytes = true) {
  const uint16_t NUM_CHANNELS = mic_array->Channels();

  try {
    for (size_t i = 0; i < NUM_CHANNELS; i++) {
      std::string topic = topic_name + "_" + std::to_string(i + 1);
      auto data = block.samples[i];

      std::stringstream ds;
      ds << "[";

      for (auto &d : data) {
        ds << d << ", ";
      }

      if (!data.empty()) {
        ds.seekp(-1, ds.cur); // Delete last space (' ')
      }
      ds << "]";

      mqtt::message_ptr pubmsg;
      if (send_bytes) {
        pubmsg = mqtt::make_message(topic,
                                    reinterpret_cast<const char *>(data.data()),
                                    data.size() * sizeof(int16_t));
      } else {
        pubmsg = mqtt::make_message(topic, ds.str());
      }

      pubmsg->set_qos(qos);
      client.publish(pubmsg);
    }
  } catch (const mqtt::exception &exc) {
    std::cerr << "Sync MQTT error" << exc.what() << std::endl;
  }
}


bool connect_async_mqtt_client(mqtt::async_client &client,
                               AsyncMQTTOptions opts) {

  using namespace std::chrono_literals;
  try {
    if (opts.connect_waiting_time_ms == -1) {
      client.connect(opts.conn_opts)->wait();
    } else {
      client.connect(opts.conn_opts)
          ->wait_for(opts.connect_waiting_time_ms * 1ms);
    }
  } catch (const mqtt::exception &exc) {
    std::cerr << "Connect async MQTT error" << exc.what() << std::endl;
    return false;
  }
  return true;
}

bool disconnect_async_mqtt_client(mqtt::async_client &client, AsyncMQTTOptions opts) {
  using namespace std::chrono_literals;

  try {
    int message_count = client.get_pending_delivery_tokens().size();
    if (opts.wait_for_unsent_messages && message_count != 0) {
      int retry_cnt = opts.num_retries_send_unsent_messages;
      while (!client.get_pending_delivery_tokens().empty()) {
        auto toks = client.get_pending_delivery_tokens();
        int new_message_count = toks.size();
        std::this_thread::sleep_for(opts.waiting_unsent_messages_time_ms * 1ms);
        if (new_message_count == message_count) {
          retry_cnt--;
        }
        if (retry_cnt == 0) {
          break;
        }
      }
    }

    if (opts.waiting_time_disconnect_ms == -1) {
      client.disconnect()->wait();
    } else {
      client.disconnect()->wait_for(opts.waiting_time_disconnect_ms * 1ms);
    }
  } catch (const mqtt::exception &exc) {
    std::cerr << "Disconnect async MQTT error" << exc.what() << std::endl;
    return false;
  }
  return true;
}

void send_audio_mqtt_async(matrix_hal::MicrophoneArray *mic_array,
                     SafeQueue<AudioBlock> &queue, std::atomic_bool &running,
                     AsyncMQTTOptions mqtt_options, bool drain = true) {

  const uint16_t NUM_CHANNELS = mic_array->Channels();

  mqtt::async_client client{mqtt_options.ip + ":" + mqtt_options.port ,mqtt_options.clientID};
  connect_async_mqtt_client(client, mqtt_options);

  while (running || (drain && !queue.empty())) {
    AudioBlock block;

    int ret = queue.wait_pop(block);
    if (ret == 0) {
      continue;
    }

    for (size_t i = 0; i < NUM_CHANNELS; i++) {
      std::string topic =
          mqtt_options.base_topic_name + "_" + std::to_string(i + 1);
      auto data_vec = block.samples[i];

      std::stringstream data_string;
      data_string << "[";

      for (auto &d : data_vec) {
        data_string << d << ", ";
      }

      if (!data_vec.empty()) {
        data_string.seekp(-1, data_string.cur); // Delete last space (' ')
      }
      data_string << "]";

      mqtt::message_ptr pubmsg;
      if (mqtt_options.send_bytes) {
        pubmsg = mqtt::make_message(
            topic, reinterpret_cast<const char *>(data_vec.data()),
            data_vec.size() * sizeof(int16_t));
      } else {
        pubmsg = mqtt::make_message(topic, data_string.str());
      }

      try {
        pubmsg->set_qos(mqtt_options.qos);
        client.publish(pubmsg);
      } catch (const mqtt::exception &exc) {
        std::cerr << "Async MQTT publish error" << exc.what() << std::endl;
      }
    }
  }

  disconnect_async_mqtt_client(client, mqtt_options);
}
