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
#include "queue.hpp"

// Variables globales
extern mqtt::async_client client;
extern const std::string BEAMFORMED_TOPIC;

// Prototipos
void capture_audio(
    matrix_hal::MicrophoneArray* mic_array,
    SafeQueue<AudioBlock>& queue,
    std::atomic_bool& running,
    int duration
);


void write_wav_header(
    std::ofstream &out,
    uint32_t sample_rate,
    uint16_t bits_per_sample,
    uint16_t num_channels,
    uint32_t data_size
);
