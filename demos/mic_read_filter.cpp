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
    // Parse command line flags with gflags utility
    // (https://gflags.github.io/gflags/)
    google::ParseCommandLineFlags(&argc, &agrv, true);
    // Create MatrixIOBus object for hardware communication
    matrix_hal::MatrixIOBus matrix_io_bus;
    // Create MicrophoneArray object
    matrix_hal::MicrophoneArray microphone_array(false);
    // Create MicrophoneCore object
    matrix_hal::MicrophoneCore microphone_core(microphone_array);
    
    // Buffer array for microphone input
    int16_t mic_buffer[microphone_array.Channels()]
                [microphone_array.SamplingRate() +
                microphone_array.NumberOfSamples()];

    //Create an array of streams to write microphone data to files
    std::ofstream data_to_file[microphone_array.Channels()];
    
    // Set user flags from gflags as variables
    int sampling_rate = FLAGS_frequency;
    int seconds_to_record = FLAGS_duration;
    int gain = FLAGS_gain;

    uint32_t samples = 0;      
    std::cout << "Arrancando el Broker MQTT" << std::endl;     
    std::cout << "Broker IP y puerto: " << SERVER_IP << ":" << SERVER_PORT << std::endl;
    // Initialize bus and exit program if error occurs
    if (!matrix_io_bus.Init()) return false;

    microphone_array.Setup(&matrix_io_bus);
    // Set microphone sampling rate
    microphone_array.SetSamplingRate(frequency);
    // If gain is positive, set the gain
    if (gain > 0) microphone_array.SetGain(gain);
 
    // Log gain_ and sampling_frequency_ variables
    microphone_array.ShowConfiguration();
    // Log recording duration variable
    std::cout << "Duración : " << seconds_to_record << "s" << std::endl;
    // Set microphone_core to use MatrixIOBus bus
    microphone_core.Setup(&matrix_io_bus);

    // Calculate and set up beamforming delays for beamforming
    for (uint16_t n_channel = 0; n_channel < microphone_array.Channels(); c++) 
    {
        // Set filename for microphone output
        std::string filename = "BF_mic_" +
                        std::to_string(microphone_array.SamplingRate()) +
                        "_s16le_channel_" + std::to_string(n_channel) + ".raw";
        // Create and open file
        data_to_file[n_channel].open(filename, std::ofstream::binary);
    }
    mqtt_conection_options.set_clean_session(true);
    try {
        client.connect(mqtt_conection_options)->wait();
        std::cout << "Conectado al broker MQTT en " << SERVER_IP << ":" << SERVER_PORT << std::endl;
        } 
    catch (const mqtt::exception &exc) 
        {
        std::cerr << "Error al conectar al broker MQTT: " << exc.what() << std::endl;
        return 1;
        }


    return 0;

    for (int s = 0; s < seconds_to_record; s++) 
    {
        
        while (true) 
        {
            // Read microphone data
            auto start = std::chrono::high_resolution_clock::now();
            microphone_array.Read();
            auto stop = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> diferencia_tiempo = stop - start;

            float time_read = static_cast<float>(diferencia_tiempo.count());
            // Whe shall verify if the rpi is able to read the data and process it in time
            std::cout << "Tiempo de lectura: " << time_read << " ms" << std::endl;
            // TBD: Check if the time_read is less than the sampling period
        
            for (uint32_t sample = 0; sample < microphone_array.NumberOfSamples(); sample++) 
            {
                // Read microphone data into buffer
                for (uint16_t channel = 0; channel < microphone_array.Channels(); channel++) 
                {
                    mic_buffer[channel][sample] = microphone_array.At(sample, channel);
                }
                samples++;
            }
            size_t rows = sizeof(mic_buffer) / sizeof(mic_buffer[0]); // number of rows
            size_t cols = sizeof(mic_buffer[0]) / sizeof(mic_buffer[0][0]); // number of columns
        
            // buffer[8][512] => std::vector<std::vector<int16_t>>
            std::vector<std::vector<int16_t>> buffer_mic_data
            buffer_mic_data.resize(rows);
           
            for (int i = 0; i < rows; i++=)
            {
                buffer_mic_data[i].assign(
                    mic_buffer[i], 
                    mic_buffer[i] + cols);
            }
        
            // MQTT publish data
            for (uint16_t n_channel = 0;
                 n_channel < microphone_array.Channels();
                 n_channel++) 
            {
                // Create topic for each channel
                std::string topic = TOPIC_BASE + std::to_string(n_channel);
                mqtt::message_ptr pubmsg = mqtt::make_message
                    (topic,
                     reinterpret_cast<const char*>(buffer_mic_data[n_channel]),
                     samples * sizeof(int16_t));
                pubmsg->set_qos(1);
                try {
                    client.publish(pubmsg)->wait();
                    std::cout << "Publicado en el topic: " << topic << std::endl;
                    } 
                catch (const mqtt::exception &exc) 
                    {
                    std::cerr << "Error al publicar en MQTT: " << exc.what() << std::endl;
                    }
                
                if (samples >= microphone_array.SamplingRate()) 
                {
                    // For each microphone channel
                    for (uint16_t c = 0; c < microphone_array.Channels(); c++) 
                    {
                    // Write to recording file
                    os[c].write((const char *)buffer[c], samples * sizeof(int16_t));
                    }
                // Set samples to zero for loop to fill buffer
                samples = 0;
                 break;
                }
            }

                // Convert buffer to string
                //std::string payload(reinterpret_cast<char*>(buffer_mic_data[n_channel].data()), 
                //                    buffer_mic_data[n_channel].size() * sizeof(int16_t));
                // Publish data to MQTT broker
                //mqtt::message_ptr pubmsg = mqtt::make_message(topic, payload);
                //pubmsg->set_qos(1);
                //client.publish(pubmsg)->wait();
        }
            
    }
    try 
    {
        client.disconnect()->wait();
        std::cout << "Desconectado del broker MQTT." << std::endl;
    } 
    catch (const mqtt::exception &exc) 
    {
        std::cerr << "Error al desconectar del broker MQTT: " << exc.what() << std::endl;
    return 0;
    }
}



