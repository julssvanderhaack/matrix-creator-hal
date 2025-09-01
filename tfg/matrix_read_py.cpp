// FILE    : matrix_read_py.cpp
// Autor   : Julio Albisua
// INFO    : coge el audio de los 8 micrófonos, los procesa por beamforming de banda estrecha,
//           enciende en Everloop el LED según el DOA, y envía el audio por MQTT
//           En este se genera una librería con pybind11 para poder implementarla en python

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
#include "queue.hpp"
// Configuración de pybind.h
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

// Configuración de MQTT
const std::string SERVER_IP    = "tcp://localhost";
const std::string SERVER_PORT  = "1883";
const std::string CLIENT_ID    = "AudioPublisher";
const std::string BEAMFORMED_TOPIC = "audio/beamformed";

mqtt::async_client client(SERVER_IP + ":" + SERVER_PORT, CLIENT_ID);
mqtt::connect_options mqtt_connection_options;
std::atomic<bool> running(true);

// Parámetros CLI

int run_matrix_read(int frequency, int duration, int gain, const std::string& filename) {

    // Inicializar bus MATRIX
    matrix_hal::MatrixIOBus bus;
    if (!bus.Init()) return 1;

    // Mic array
    matrix_hal::MicrophoneArray mic_array(false);
    matrix_hal::MicrophoneCore  mic_core(mic_array);
    mic_array.Setup(&bus);
    mic_array.SetSamplingRate(frequency);
   
    if (gain > 0) mic_array.SetGain(gain);

    mic_array.ShowConfiguration();
    mic_core.Setup(&bus);

    // Conectar MQTT
    mqtt_connection_options.set_clean_session(true);
    try {
        client.connect(mqtt_connection_options)->wait();
        std::cout << "Conectado al broker MQTT." << std::endl;
    } catch (const mqtt::exception &exc) {
        std::cerr << "Error al conectar MQTT: " << exc.what() << std::endl;
        return 1;
    }

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
        duration
    );

    // Hilo de beamforming + Everloop
    std::thread processing_thread(
        process_beamforming,
        std::ref(queue),
        frequency,
        duration,
        &everloop,
        &image,
        filename);

    // Esperar hilos
    capture_thread.join();
    running = false;
    processing_thread.join();

    // Desconectar MQTT
    try {
        client.disconnect()->wait();
        std::cout << "Desconectado del broker MQTT." << std::endl;
    } catch (const mqtt::exception &exc) {
        std::cerr << "Error al desconectar MQTT: " << exc.what() << std::endl;
    }

    return 0;
}
PYBIND11_MODULE(matrix_hal_tfg, tfg) {
    tfg.doc() = "Bindings pybind11 para este proyecto con MATRIX Creator ";
    tfg.def("run",
            [](int frequency, int duration, int gain, const std::string& filename) {
                py::gil_scoped_release release;  // no bloquear Python
                return run_matrix_read(frequency, duration, gain, filename);
            },
            py::arg("frequency") = 16000,
            py::arg("duration")  = 5,
            py::arg("gain")      = 3,
            py::arg("filename")  = "beamformed_output.wav",
        "   Ejecuta el pipeline completo; devuelve 0 si OK."
        );
}
