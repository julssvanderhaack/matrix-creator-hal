// matrix_read.cpp
// Autor   : Julio Albisua
// INFO    : coge el audio de los 8 micrófonos, los procesa por beamforming de banda estrecha,
//           enciende en Everloop el LED según el DOA, y envía el audio por MQTT

#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <gflags/gflags.h>
#include <mqtt/async_client.h>

#include "../cpp/driver/matrixio_bus.h"
#include "../cpp/driver/microphone_array.h"
#include "../cpp/driver/microphone_core.h"
#include "../cpp/driver/everloop.h"
#include "../cpp/driver/everloop_image.h"

#include "audio_processor.hpp"
#include "utils.hpp"

std::atomic<bool> running(true);

// Parámetros CLI
DEFINE_int32(frequency, 16000, "Frecuencia de muestreo (Hz)");
DEFINE_int32(duration, 0, "Segundos a grabar (por defecto, 0=continuo). De momento no guarda las cosas en grabacion continua");
DEFINE_int32(gain, 3, "Ganancia del micrófono (dB)");
DEFINE_string(filename, "recording", "The filename of the recorded files");
DEFINE_string(folder, "./", "The filename of the recorded files");

int main(int argc, char *argv[])
{
    // Mensaje de ayuda personalizado
    google::SetUsageMessage(
        "Uso:\n"
        "  matrix_read --frequency=<Hz> --duration=<s> --gain=<dB>\n"
        "\n"
        "Parámetros:\n"
        "  --frequency : Frecuencia de muestreo en Hz (por defecto: 16000)\n"
        "  --duration  : Duración en segundos de la grabación (por defecto: 5)\n"
        "                   Si se pone duration 0 el programa correrá de forma continua\n"
        "  --gain      : Ganancia del micrófono en dB, 3 para ganancia por defecto (por defecto: 3)\n"
        "  --filename  : The filename of the recorded files\n"
        "  --folder    : The folder, indicating where to save the recorded files (default: current directory)\n");

    for (int i = 1; i < argc; ++i)
    {
        std::string a(argv[i]);
        if (a == "--help" || a == "-h")
        {
            std::cout << google::ProgramUsage() << std::endl;
            return 0;
        }
    }
    google::ParseCommandLineFlags(&argc, &argv, true);

    // Inicializar bus MATRIX
    matrix_hal::MatrixIOBus bus;
    if (!bus.Init())
        return 1;

    // Mic array
    matrix_hal::MicrophoneArray mic_array(false);
    matrix_hal::MicrophoneCore mic_core(mic_array);
    mic_array.Setup(&bus);
    mic_array.SetSamplingRate(FLAGS_frequency);
    if (FLAGS_gain > 0)
    {
        mic_array.SetGain(FLAGS_gain);
    }

    mic_array.ShowConfiguration();
    mic_core.Setup(&bus);

    // Everloop
    matrix_hal::Everloop everloop;
    everloop.Setup(&bus);
    matrix_hal::EverloopImage image(bus.MatrixLeds());

    // Cola de audio
    SafeQueue<AudioBlock> queue;

    running = true;

    // Hilo de captura
    std::thread capture_thread(
        capture_audio,
        &mic_array,
        std::ref(queue),
        FLAGS_duration);

    // Hilo de beamforming + Everloop
    std::thread processing_thread(
        record_all_channels_raw,
        std::ref(queue),
        FLAGS_frequency,
        FLAGS_duration,
        FLAGS_folder,
        FLAGS_filename);

    // Esperar hilos
    capture_thread.join();
    running = false; // Signal the recording thread to stop
    processing_thread.join();

    return 0;
}
