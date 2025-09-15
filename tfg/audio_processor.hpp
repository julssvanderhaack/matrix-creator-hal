// FILE    : audio_processor.hpp
// Autor   : Julio Albisua
// INFO    : En este código se procesa el audio de los micrófonos de la rpi
//           Se retrasmite el audio por mqtt en el canal audio/beanformed
//           y se enciende el led más cercano a la DOA calculada

#include "../cpp/driver/microphone_array.h"
#include "mqtt/client.h"
#include "queue.hpp"
#include <atomic>

struct AsyncMQTTOptions {
public:
  std::string ip;
  std::string port;
  std::string clientID;
  std::string base_topic_name;
  mqtt::connect_options conn_opts;
  int qos;
  bool send_bytes;
  int connect_waiting_time_ms;
  int waiting_time_disconnect_ms;
  bool wait_for_unsent_messages;
  int waiting_unsent_messages_time_ms;
  int num_retries_send_unsent_messages;

public:
  AsyncMQTTOptions()
      : ip{"127.0.0.1"}, port{"1883"}, clientID{"AsyncMatrixPublisher"},
        base_topic_name{"audio_ch"}, conn_opts{mqtt::connect_options{}}, qos{1},
        send_bytes{true}, connect_waiting_time_ms{-1},
        waiting_time_disconnect_ms{-1}, wait_for_unsent_messages{true},
        waiting_unsent_messages_time_ms{250},
        num_retries_send_unsent_messages{5} {
    conn_opts.set_clean_session(true);
  }

  void set_ip(std::string ip) { this->ip = ip; }

  void set_port(std::string port) { this->port = port; }

  void set_id(std::string id) { this->clientID = id; }

  void set_topic_name(std::string name) { this->base_topic_name = name; }

  void set_connect_options(mqtt::connect_options opts) {
    this->conn_opts = opts;
  }

  void set_qos(int qos) { this->qos = qos; }

  void set_send_bytes(bool send_bytes) { this->send_bytes = send_bytes; }

  void set_connect_timeout(int timeout) {
    this->connect_waiting_time_ms = timeout;
  }

  void set_disconnect_timeout(int timeout) {
    this->waiting_time_disconnect_ms = timeout;
  }

  void set_wait_for_unsent_messages(bool wait) {
    this->wait_for_unsent_messages = wait;
  }

  void set_unsent_messages_timeout(int timeout) {
    this->waiting_unsent_messages_time_ms = timeout;
  }

  void set_retries_send_unsent_messages(int retries) {
    if (retries == 0) {
      retries = 1; // We want to do at least a first attempt to send
    }
    this->num_retries_send_unsent_messages = retries;
  }
};

void capture_audio(matrix_hal::MicrophoneArray *mic_array,
                   SafeQueue<AudioBlock> &queue, std::atomic_bool &running);

void write_wav_header(std::ofstream &out, uint32_t sample_rate,
                      uint16_t bits_per_sample, uint16_t num_channels,
                      uint32_t data_size);

void send_audio_mqtt_async(matrix_hal::MicrophoneArray *mic_array,
                           SafeQueue<AudioBlock> &queue,
                           std::atomic_bool &running,
                           AsyncMQTTOptions mqtt_options, bool drain);

void send_audio_mqtt_sync(mqtt::client &client,
                          matrix_hal::MicrophoneArray *mic_array,
                          AudioBlock block, std::string topic_name, int qos,
                          bool send_bytes);

bool disconnect_sync_mqtt_client(mqtt::client &client);

bool connect_sync_mqtt_client(mqtt::client &client,
                              mqtt::connect_options *conn_opts);

void record_all_channels_wav_sync(matrix_hal::MicrophoneArray *mic_array,
                                  AudioBlock data,
                                  std::string filename_without_extension);

AudioBlock capture_audio_sync(matrix_hal::MicrophoneArray *mic_array);

void record_all_channels_wav(SafeQueue<AudioBlock> &queue,
                             matrix_hal::MicrophoneArray *mic_array,
                             std::atomic_bool &running,
                             std::string filename_without_extension,
                             bool drain);
