// FILE   : test_record_sync.cpp
// AUTHOR : Julio Albisua
// INFO   : captures the audio of all microphones 
// 	    saves it in the volatile memory	   

#include "../cpp/driver/matrixio_bus.h"
#include "../cpp/driver/microphone_array.h"
#include "../cpp/driver/microphone_core.h"
#include "audio_processor.hpp"

int main() {
  auto duration = 5;
  auto frequency = 16000;
  auto gain = 0;
  std::string BASE_FILENAME = "test_record_sync";

  auto end_time =
      std::chrono::steady_clock::now() + std::chrono::seconds(duration);

  std::cerr << "Testing record sync with: " << frequency
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

  while (duration == 0 || std::chrono::steady_clock::now() < end_time) {
    auto b = capture_audio_sync(&mic_array);
    record_all_channels_wav_sync(&mic_array, b, BASE_FILENAME);
  }
}
