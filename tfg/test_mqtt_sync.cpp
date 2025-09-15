#include "../cpp/driver/matrixio_bus.h"
#include "../cpp/driver/microphone_array.h"
#include "../cpp/driver/microphone_core.h"
#include "audio_processor.hpp"
#include "mqtt/client.h"

int main() {
  auto duration = 5;
  auto frequency = 16000;
  auto gain = 0;

  std::string SERVER_IP = "tcp://localhost";
  std::string SERVER_PORT = "1883";
  std::string CLIENT_ID = "AudioPublisher";
  std::string BASE_TOPIC_NAME = "test_mqtt";

  std::cerr << "Testing mqtt audio send sync with: " << frequency
            << " Hz, gain = " << gain << " during " << duration << " s "
            << " to IP:" << SERVER_IP << " port " << SERVER_PORT
            << " with client id " << CLIENT_ID << " with base topic name "
            << BASE_TOPIC_NAME << std::endl;

  auto end_time =
      std::chrono::steady_clock::now() + std::chrono::seconds(duration);

  // Inicializar bus MATRIX
  matrix_hal::MatrixIOBus bus;
  if (!bus.Init()) {
    return 1;
  }

  // Mic array
  matrix_hal::MicrophoneArray mic_array(false);
  matrix_hal::MicrophoneCore mic_core(mic_array);
  mic_array.Setup(&bus);
  mic_array.SetSamplingRate(frequency);

  if (gain > 0) {
    mic_array.SetGain(gain);
  }

  mic_array.ShowConfiguration();
  mic_core.Setup(&bus);

  mqtt::client client(SERVER_IP + ":" + SERVER_PORT, CLIENT_ID);
  connect_sync_mqtt_client(client, nullptr);

  while (duration == 0 || std::chrono::steady_clock::now() < end_time) {
    auto b = capture_audio_sync(&mic_array);
    send_audio_mqtt_sync(client, &mic_array, b, BASE_TOPIC_NAME, 1, true);
  }
  disconnect_sync_mqtt_client(client);
}
