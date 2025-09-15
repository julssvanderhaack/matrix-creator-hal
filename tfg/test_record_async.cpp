// FILE   : test_record_async.cpp
// AUTHOR : Julio Albisua
// INFO   : captures the audio of all microphones 
// 	    saves it in the volatile memory	    

#include "../cpp/driver/matrixio_bus.h"
#include "../cpp/driver/microphone_array.h"
#include "../cpp/driver/microphone_core.h"
#include "audio_processor.hpp"
#include "queue.hpp"
#include <thread>

using namespace std::chrono_literals;

int main() {
  auto duration = 5;
  auto frequency = 16000;
  auto gain = 0;
  std::string BASE_FILENAME = "test_record_async";

  std::cerr << "Testing record async with: " << frequency
            << " Hz, gain = " << gain << " during " << duration << " s "
            << " with filenames " << BASE_FILENAME << std::endl;

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

  SafeQueue<AudioBlock> q{};
  q.start_async();
  std::thread prod{capture_audio, &mic_array, std::ref(q),
                   std::ref(q.run_async)};
  std::thread cons{record_all_channels_wav, std::ref(q),   &mic_array,
                   std::ref(q.run_async),   BASE_FILENAME, true};

  std::this_thread::sleep_for(duration * 1s);
  q.stop_async();
  prod.join();
  cons.join();
  return 0;
}
