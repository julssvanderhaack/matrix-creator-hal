// FILE    : audio_processor.hpp
// Autor   : Julio Albisua
// INFO    : En este código se procesa el audio de los micrófonos de la rpi
//           Se retrasmite el audio por mqtt en el canal audio/beanformed
//           y se enciende el led más cercano a la DOA calculada

#include <vector>
#include <atomic>
#include "../cpp/driver/microphone_array.h"
#include "../cpp/driver/everloop.h"
#include "../cpp/driver/everloop_image.h"
#include "mqtt/async_client.h"
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

void write_wav_header(
    std::ofstream &out,
    uint32_t sample_rate,
    uint16_t bits_per_sample,
    uint16_t num_channels,
    uint32_t data_size
);
