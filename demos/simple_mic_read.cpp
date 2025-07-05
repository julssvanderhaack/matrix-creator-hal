// Google gflags parse
//
#include <gflags/gflags.h>
// Communicating with Pi GPIO
#include <wiringPi.h>
// Input/output stream class to operate on files
#include <fstream>
// Input/output streams and functions
#include <iostream>
// Use strings
#include <string>
// Arrays for math operations
#include <valarray>
#include <chrono>

// Communicates with MATRIX device
#include "../cpp/driver/matrixio_bus.h"
// Interfaces with microphone array
#include "../cpp/driver/microphone_array.h"
// Enables using FIR filter with microphone array
#include "../cpp/driver/microphone_core.h"

#include "mqtt/async_client.h"

//#include "zero-gcc-phat/src/zo-fft.hpp"
//#include "zero-gcc-phat/src/zo-gcc-phat.hpp"

// MQTT Configuration
const std::string SERVER_ADDRESS("tcp://localhost:1883"); // Cambiar por la dirección del broker MQTT
const std::string CLIENT_ID("AudioPublisher");
const std::string TOPIC_BASE("audio/channel_");

// Crear cliente MQTT
mqtt::async_client client(SERVER_ADDRESS, CLIENT_ID);
mqtt::connect_options connOpts;

// Defines variables from user arguments using gflags utility
// (https://gflags.github.io/gflags/)

// Grabs sampling frequency input from user
DEFINE_int32(sampling_frequency, 48000, "Sampling Frequency");  // Argument example: "--sampling_frequency 48000"
// Grabs duration input from user
DEFINE_int32(duration, 5, "Interrupt after N seconds"); // Argument example: "--duration 10"
// Grabs gain input from user
DEFINE_int32(gain, -1, "Microphone Gain"); // Argument example: "--gain 5"

int main(int argc, char *agrv[]) {
    // Parse command line flags with gflags utility
    // (https://gflags.github.io/gflags/)
    google::ParseCommandLineFlags(&argc, &agrv, true);

    // Create MatrixIOBus object for hardware communication
    matrix_hal::MatrixIOBus bus;
    // Initialize bus and exit program if error occurs
    if (!bus.Init()) return false;

    // Set user flags from gflags as variables
    int sampling_rate = FLAGS_sampling_frequency;
    int seconds_to_record = FLAGS_duration;
    int gain = FLAGS_gain;
    // The following code is part of main()

    // Create MicrophoneArray object
    matrix_hal::MicrophoneArray microphone_array(false);
    // Set microphone_array to use MatrixIOBus bus
    microphone_array.Setup(&bus);
    // Set microphone sampling rate
    microphone_array.SetSamplingRate(sampling_rate);
    // If gain is positive, set the gain
    if (gain > 0) microphone_array.SetGain(gain);

    // Log gain_ and sampling_frequency_ variables
    microphone_array.ShowConfiguration();
    // Log recording duration variable
    std::cout << "Duration : " << seconds_to_record << "s" << std::endl;

    // Calculate and set up beamforming delays for beamforming
    //microphone_array.CalculateDelays(0, 0, 1000, 320 * 1000);  // These are default values
    // The following code is part of main()

    // Create MicrophoneCore object
    matrix_hal::MicrophoneCore microphone_core(microphone_array);
    // Set microphone_core to use MatrixIOBus bus
    microphone_core.Setup(&bus);
    // The following code is part of main()

    // Create a buffer array for microphone input
    int16_t buffer[microphone_array.Channels()]
                [microphone_array.SamplingRate() +
                microphone_array.NumberOfSamples()];


    // Create a buffer array for microphone input
    // int16_t buffer[microphone_array.Channels()]
    //                 [microphone_array.NumberOfSamples()];


    //Create an array of streams to write microphone data to files
    std::ofstream os[microphone_array.Channels()];

    // For each microphone channel (+1 for beamforming), make a file and open it
for (uint16_t c = 0; c < microphone_array.Channels(); c++) {
    // Set filename for microphone output
    std::string filename = "mic_" +
                        std::to_string(microphone_array.SamplingRate()) +
                        "_s16le_channel_" + std::to_string(c) + ".raw";
    // Create and open file
    os[c].open(filename, std::ofstream::binary);
}

// // ZERO - GCC PHAT algoritm
//zo::GccPhat* gcc_phat = zo::GccPhat::create();

//int margin = 50;

    // Configurar opciones de conexión MQTT
    connOpts.set_clean_session(true);
    try {
        client.connect(connOpts)->wait();
        std::cout << "Conectado al broker MQTT en " << SERVER_ADDRESS << std::endl;
    } catch (const mqtt::exception &exc) {
        std::cerr << "Error al conectar al broker MQTT: " << exc.what() << std::endl;
        return 1;
    }

    // Counter variable for tracking recording time
    uint32_t samples = 0;
    // For recording duration
    for (int s = 0; s < seconds_to_record; s++) {
        // Endless loop
        while (true) {
    // Read microphone stream data

    // Read microphone stream data
    auto start = std::chrono::high_resolution_clock::now();
    microphone_array.Read();

    auto stop = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> diferencia_tiempo = stop - start;

    float Ts = static_cast<float>(diferencia_tiempo.count());
    std::cout << "Tiempo de lectura MIC: " << Ts << " miliseconds" << std::endl;



    // For number of samples
    for (uint32_t s = 0; s < microphone_array.NumberOfSamples(); s++) {
        // For each microphone
        for (uint16_t c = 0; c < microphone_array.Channels(); c++) {
        // Send microphone data to buffer
        buffer[c][samples] = microphone_array.At(s, c);
        }
        // Writes beamformed microphone data into buffer
        //buffer[microphone_array.Channels()][samples] = microphone_array.Beam(s);
        // Increment samples for buffer write
        samples++;
    }


    size_t rows = sizeof(buffer) / sizeof(buffer[0]); // number of rows
    size_t cols = sizeof(buffer[0]) / sizeof(buffer[0][0]); // number of columns

    // Convertir buffer[8][512] en un std::vector<std::vector<int16_t>>
    std::vector<std::vector<int16_t>> buffer_vec;
    buffer_vec.resize(rows);
    for (size_t i = 0; i < rows; i++) {
        buffer_vec[i].assign(buffer[i], buffer[i] + cols);
    }

    //gcc_phat->init(cols);

    //float distance = gcc_phat->execute_gcc(buffer_vec,margin);

     //std::cout << "Ángulo de incidendia de la fuente en array UCA: " << distance << "\n";

    // Publicar datos de audio por MQTT
    for (uint16_t c = 0; c < microphone_array.Channels(); c++) {
        std::string topic = TOPIC_BASE + std::to_string(c);
        mqtt::message_ptr pubmsg = mqtt::make_message(
            topic,
            reinterpret_cast<const char *>(buffer[c]),
            samples * sizeof(int16_t)
        );
        pubmsg->set_qos(1);
        try {
            client.publish(pubmsg);
        } catch (const mqtt::exception &exc) {
            std::cerr << "Error al publicar en MQTT: " << exc.what() << std::endl;
        }
    }

    //Once number of samples is >= sampling rate
     if (samples >= microphone_array.SamplingRate()) {
        // For each microphone channel
         for (uint16_t c = 0; c < microphone_array.Channels(); c++) {
         // Write to recording file
         os[c].write((const char *)buffer[c], samples * sizeof(int16_t));
         }
         // Set samples to zero for loop to fill buffer
         samples = 0;
         break;
      }
     }
 }

//delete gcc_phat;

// Desconectar cliente MQTT
try {
    client.disconnect()->wait();
    std::cout << "Desconectado del broker MQTT." << std::endl;
} catch (const mqtt::exception &exc) {
    std::cerr << "Error al desconectar del broker MQTT: " << exc.what() << std::endl;
}

return 0;
}

