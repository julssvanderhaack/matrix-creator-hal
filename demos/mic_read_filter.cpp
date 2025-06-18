// FILE        : mic_read_filter.cpp
// AUTHOR      : Julio Albisua julioalbisua@gmail.com
// DESCRIPTION : This program reads microphone data from the Matrix Creator board
//               and applies a filter to be selected by the user in the command line.
//               The filtered data is then published to an MQTT broker.
//               The program uses the Matrix Creator HAL library and the Paho MQTT C++
//               library.
//               Also it uses beamforming 

//Includes 

#include <gflags/gflags.h> // Command line flags
#include <wiringPi.h> 
#include <iostream> // Standard I/O
#include <fstream> // Input/output streams and functions
#include <string> // Use strings

#include "matrix_hal/matrixio_bus.h"     // Communicates with MATRIX device
#include "matrix_hal/microphone_array.h" // Interfaces with microphone array
#include "matrix_hal/microphone_core.h"  // Enables using FIR filter with microphone array
#include "mqtt/async_client.h" // MQTT C++ library

//Constants for MQTT configuration
const std::string SERVER_IP("tcp://localhost");
const std::string SERVER_PORT("1883");
const std::string CLIENT_ID("AudioPublisher");
const std::string TOPIC_BASE("audio/channel_");
// TBD: beanforming implementation

mqtt::async_client client(SERVER_IP + ":" + SERVER_PORT, CLIENT_ID);
mqtt::connect_options mqtt_conection_options;


DEFINE_int32(frequency, 48000, 
            "Sampling Frequency");  
    // Argument example: "--frequency 48000"

DEFINE_int32(duration, 5, 
            "Interrupt after N seconds"); 
    // Grabs duration input from user// Argument example: "--duration 10"

DEFINE_int32(gain, -1,
            "Microphone Gain"); 
    // Argument example: "--gain 5"

    //void init() 
    //TBD when the init in main is implemented

int main(int argc, char *argv[]) 
{
    // Parse flags
    google::ParseCommandLineFlags(&argc, &argv, true);

    // MATRIX Creator bus & mic
    matrix_hal::MatrixIOBus matrix_io_bus;
    matrix_hal::MicrophoneArray microphone_array(false);
    matrix_hal::MicrophoneCore microphone_core(microphone_array);

    // Configuración MQTT
    mqtt::async_client client(SERVER_IP + ":" + SERVER_PORT, CLIENT_ID);
    mqtt::connect_options mqtt_conection_options;
    mqtt_conection_options.set_clean_session(true);

    // Flags del usuario
    int frequency = FLAGS_frequency;
    int seconds_to_record = FLAGS_duration;
    int gain = FLAGS_gain;

    std::cout << "Arrancando el Broker MQTT" << std::endl;     
    std::cout << "Broker IP y puerto: " << SERVER_IP << ":" << SERVER_PORT << std::endl;

    if (!matrix_io_bus.Init()) return false;

    microphone_array.Setup(&matrix_io_bus);
    microphone_array.SetSamplingRate(frequency);
    if (gain > 0) microphone_array.SetGain(gain);
    microphone_array.ShowConfiguration();
    microphone_core.Setup(&matrix_io_bus);

    std::cout << "Fin de la inicialización" << std::endl; 

    // Crear archivos de salida
    std::ofstream data_to_file[microphone_array.Channels()];
    for (uint16_t ch = 0; ch < microphone_array.Channels(); ++ch) {
        std::string filename = "BF_mic_" + std::to_string(frequency) + "_s16le_channel_" + std::to_string(ch) + ".raw";
        data_to_file[ch].open(filename, std::ofstream::binary);
        if (!data_to_file[ch]) {
            std::cerr << "Error creando archivo: " << filename << std::endl;
            return 1;
        }
        std::cout << "Creando fichero: " << filename << std::endl; 
    }

    // Conectar MQTT
    try {
        client.connect(mqtt_conection_options)->wait();
        std::cout << "Conectado al broker MQTT en " << SERVER_IP << ":" << SERVER_PORT << std::endl;
    } catch (const mqtt::exception &exc) {
        std::cerr << "Error al conectar al broker MQTT: " << exc.what() << std::endl;
        return 1;
    }

    // Variables de buffer
    const uint32_t SAMPLES_PER_SECOND = frequency;
    const uint32_t BLOCK_SIZE = microphone_array.NumberOfSamples();
    int16_t mic_buffer[microphone_array.Channels()][SAMPLES_PER_SECOND];

    // Bucle principal: grabar cada segundo
    for (int s = 0; s < seconds_to_record; ++s) {
        std::cout << "\nGrabando segundo: " << s + 1 << " de " << seconds_to_record << std::endl;

        uint32_t samples = 0;
        while (samples < SAMPLES_PER_SECOND) {
            microphone_array.Read();
            uint32_t num_samples = BLOCK_SIZE;

            // Ajustar si queda menos espacio
            if (samples + num_samples > SAMPLES_PER_SECOND) {
                num_samples = SAMPLES_PER_SECOND - samples;
            }

            // Copiar al buffer
            for (uint32_t i = 0; i < num_samples; ++i) {
                for (uint16_t ch = 0; ch < microphone_array.Channels(); ++ch) {
                    mic_buffer[ch][samples + i] = microphone_array.At(i, ch);
                }
            }

            samples += num_samples;
        }

        // Guardar y publicar
        for (uint16_t ch = 0; ch < microphone_array.Channels(); ++ch) {
            // Guardar archivo
            data_to_file[ch].write(
                reinterpret_cast<const char *>(mic_buffer[ch]),
                SAMPLES_PER_SECOND * sizeof(int16_t)
            );
            data_to_file[ch].flush();
            std::cout << "Guardado canal " << ch << std::endl;

            // Publicar MQTT
            std::string topic = TOPIC_BASE + std::to_string(ch);
            mqtt::message_ptr pubmsg = mqtt::make_message(
                topic,
                reinterpret_cast<const char *>(mic_buffer[ch]),
                SAMPLES_PER_SECOND * sizeof(int16_t)
            );
            pubmsg->set_qos(1);
            try {
                client.publish(pubmsg);
                std::cout << "Publicado en MQTT canal " << ch << std::endl;
            } catch (const mqtt::exception &exc) {
                std::cerr << "Error publicando MQTT: " << exc.what() << std::endl;
            }
        }
    }

    // Desconectar MQTT
    try {
        client.disconnect()->wait();
        std::cout << "Desconectado del broker MQTT." << std::endl;
    } catch (const mqtt::exception &exc) {
        std::cerr << "Error al desconectar MQTT: " << exc.what() << std::endl;
    }

    // Cerrar archivos
    for (uint16_t ch = 0; ch < microphone_array.Channels(); ++ch) {
        data_to_file[ch].close();
    }

    return 0;
}




