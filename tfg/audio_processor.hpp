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
#include "utils.hpp"

// Variables globales
extern std::atomic<bool> running;
extern mqtt::async_client client;
extern const std::string BEAMFORMED_TOPIC;

// Prototipos
void capture_audio(
    matrix_hal::MicrophoneArray* mic_array,
    SafeQueue<AudioBlock>& queue,
    int duration
);

void process_beamforming(
    SafeQueue<AudioBlock>& queue,
    uint32_t frequency,
    int duration,
    matrix_hal::Everloop* everloop,
    matrix_hal::EverloopImage* image,
    std::string filename
);
