#include "../cpp/driver/matrixio_bus.h"
#include "../cpp/driver/microphone_array.h"
#include "../cpp/driver/microphone_core.h"
#include "audio_processor.hpp"
#include <thread>

using namespace std::chrono_literals;

int main() {
  auto duration = 5;
  auto frequency = 16000;
  auto gain = 0;

  std::string SERVER_IP = "tcp://localhost";
  std::string SERVER_PORT = "1883";
  std::string CLIENT_ID = "AudioPublisher";
  std::string BASE_TOPIC_NAME = "test_mqtt";

  std::cerr << "Testing mqtt audio send async with: " << frequency
            << " Hz, gain = " << gain << " during " << duration << " s "
            << " to IP:" << SERVER_IP << " port " << SERVER_PORT
            << " with client id " << CLIENT_ID << " with base topic name "
            << BASE_TOPIC_NAME << std::endl;

  // Matrix bus
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

  AsyncMQTTOptions mqtt_opts{};
  mqtt_opts.set_topic_name(BASE_TOPIC_NAME);
  mqtt_opts.set_send_bytes(true);
  mqtt_opts.set_ip(SERVER_IP);
  mqtt_opts.set_port(SERVER_PORT);
  mqtt_opts.set_id(CLIENT_ID);

  SafeQueue<AudioBlock> q{};
  q.start_async();
  std::thread prod{capture_audio, &mic_array, std::ref(q),
                   std::ref(q.run_async)};
  std::thread cons{send_audio_mqtt_async, &mic_array, std::ref(q),
                   std::ref(q.run_async), mqtt_opts,  true};

  std::this_thread::sleep_for(duration * 1s);

  q.stop_async();
  prod.join();
  cons.join();
}
