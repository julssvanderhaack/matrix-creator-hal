// FILE        : mic_read_filter.cpp
// AUTHOR      : Julio Albisua julioalbisua@gmail.com
// DESCRIPTION : This program reads microphone data from the Matrix Creator board
//               and applies a filter to be selected by the user in the command line.
//               The filtered data is then published to an MQTT broker.
//               The program uses the Matrix Creator HAL library and the Paho MQTT C++
//               library.


//Includes 

#include <gflags/gflags.h> // Command line flags
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

mqtt::async_client client(SERVER_IP + ":" + SERVER_PORT, CLIENT_ID);
mqtt::connect_options connOpts;


DEFINE_int32(frequency, 48000, 
            "Sampling Frequency");  
    // Argument example: "--sampling_frequency 48000"

DEFINE_int32(duration, 5, 
            "Interrupt after N seconds"); 
    // Grabs duration input from user// Argument example: "--duration 10"

DEFINE_int32(gain, -1,
            "Microphone Gain"); 
    // Argument example: "--gain 5"

DEFINE_string(filter, "none", 
    "Type of filter to apply to the audio (options: none, TBD)");
    //Usage example: --filter_type=lowpass


//void init() 
//TBD when the init in main is implemented

int main(int argc, char *argv[]) 
{
    // Parse command line flags with gflags utility
    // (https://gflags.github.io/gflags/)
    google::ParseCommandLineFlags(&argc, &agrv, true);
    // Create MatrixIOBus object for hardware communication
    matrix_hal::MatrixIOBus bus;
        // Set user flags from gflags as variables
    int sampling_rate = FLAGS_frequency;
    int seconds_to_record = FLAGS_duration;
    int gain = FLAGS_gain;
    string filter_type = FLAGS_filter;

    std::cout << "Arrancando el Broker MQTT" << std::endl;     
    std::cout << "Broker IP y puerto: " << SERVER_IP << ":" << SERVER_PORT << std::endl;
    // Initialize bus and exit program if error occurs
    if (!bus.Init()) return false;

    microphone_array.Setup(&bus);
    // Set microphone sampling rate
    microphone_array.SetSamplingRate(frequency);
    // If gain is positive, set the gain
    if (gain > 0) microphone_array.SetGain(gain);
 
    // Log gain_ and sampling_frequency_ variables
    microphone_array.ShowConfiguration();
    // Log recording duration variable
    std::cout << "Duration : " << seconds_to_record << "s" << std::endl;


    return 0;

}
