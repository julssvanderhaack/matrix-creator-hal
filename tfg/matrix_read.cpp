// ARCHIVO : matrix_read.cpp
// Autor   : Julio Albisua
// INFO    : coge el audio de los 8 micrófonos, los procesa por beamforming de banda estrecha
//           y envia el audio por mqtt 



#include <iostream>
#include <vector>
#include <gflags/gflags.h>
#include <mqtt/async_client.h>
#include "matrix_hal/matrixio_bus.h"
#include "matrix_hal/microphone_array.h"
#include "matrix_hal/microphone_core.h"
#include "audio_processor.hpp"
#include "utils.hpp"

// Configuración de MQTT
const std::string SERVER_IP("tcp://localhost");
const std::string SERVER_PORT("1883");
const std::string CLIENT_ID("AudioPublisher");
const std::string BEAMFORMED_TOPIC("audio/beamformed");

mqtt::async_client client(SERVER_IP + ":" + SERVER_PORT, CLIENT_ID);
mqtt::connect_options mqtt_conection_options;
std::atomic<bool> running(true);

// Definición de parámetros configurables desde línea de comandos
DEFINE_int32(frequency, 48000, "Frecuencia de muestreo (Hz)");
DEFINE_int32(duration, 5, "Segundos a grabar");
DEFINE_int32(gain, 3, "Ganancia del micrófono");

int main(int argc, char *argv[]) {
    // Mensaje de ayuda personalizado
    google::SetUsageMessage(
        "Uso:\n"
        "  matrix_read --frequency=<Hz> --duration=<s> --gain=<dB>\n"
        "\n"
        "Parámetros:\n"
        "  --frequency : Frecuencia de muestreo en Hz (por defecto: 48000)\n"
        "  --duration  : Duración en segundos de la grabación (por defecto: 5)\n"
	"                   Si se pone duration 0 el programa correrá de forma continua\n"
        "  --gain      : Ganancia del micrófono en dB, 3 para ganancia por defecto (por defecto: 3)\n"
    );

    // Procesar parámetros --help/-h antes de gflags
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--help" || arg == "-h") {
            std::cout << google::ProgramUsage() << std::endl;
            return 0;
        }
    }
    google::ParseCommandLineFlags(&argc, &argv, true);

    // Inicialización del bus de la MATRIX Creator
    matrix_hal::MatrixIOBus bus;
    matrix_hal::MicrophoneArray mic_array(false);
    matrix_hal::MicrophoneCore mic_core(mic_array);

    if (!bus.Init()) return 1;

    int freq = FLAGS_frequency;
    int duration = FLAGS_duration;
    int gain = FLAGS_gain;

    // Configuración de la matriz de micrófonos
    mic_array.Setup(&bus);
    mic_array.SetSamplingRate(freq);
    if (gain > 0) mic_array.SetGain(gain);
    mic_array.ShowConfiguration();
    mic_core.Setup(&bus);

    // Conexión al broker MQTT
    mqtt_conection_options.set_clean_session(true);
    try {
        client.connect(mqtt_conection_options)->wait();
        std::cout << "Conectado al broker MQTT." << std::endl;
    } catch (const mqtt::exception& exc) {
        std::cerr << "Error al conectar MQTT: " << exc.what() << std::endl;
        return 1;
    }

    // Cola thread-safe para intercambiar bloques entre captura y procesamiento
    SafeQueue<AudioBlock> queue;

    // Lanzar hilo de captura
    std::thread capture_thread(capture_audio, &mic_array, std::ref(queue), duration);

    // Lanzar hilo de procesamiento y beamforming
    std::thread processing_thread(process_beamforming, std::ref(queue), freq, duration);

    // Esperar a que ambos hilos terminen
    capture_thread.join();
    running = false;  // Señal de parada al procesamiento
    processing_thread.join();

    // Desconectar MQTT
    try {
        client.disconnect()->wait();
        std::cout << "Desconectado del broker MQTT." << std::endl;
    } catch (const mqtt::exception& exc) {
        std::cerr << "Error al desconectar MQTT: " << exc.what() << std::endl;
    }

    return 0;
}