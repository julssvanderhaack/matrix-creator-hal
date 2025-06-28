#include <iostream>
#include <fstream>
#include <vector>
#include <gflags/gflags.h>
#include <mqtt/async_client.h>
#include "matrix_hal/matrixio_bus.h"
#include "matrix_hal/microphone_array.h"
#include "matrix_hal/microphone_core.h"
#include "audio_processor.hpp"
#include "thread_manager.hpp"
#include "utils.hpp"

// Config MQTT
const std::string SERVER_IP("tcp://localhost");
const std::string SERVER_PORT("1883");
const std::string CLIENT_ID("AudioPublisher");
const std::string TOPIC_BASE("audio/channel_");

mqtt::async_client client(SERVER_IP + ":" + SERVER_PORT, CLIENT_ID);
mqtt::connect_options mqtt_conection_options;
std::atomic<bool> running(true);

DEFINE_int32(frequency, 48000, "Sampling Frequency");
DEFINE_int32(duration, 5, "Seconds to record");
DEFINE_int32(gain, -1, "Microphone Gain");

int main(int argc, char *argv[]) {
    google::ParseCommandLineFlags(&argc, &argv, true);

    matrix_hal::MatrixIOBus bus;
    matrix_hal::MicrophoneArray mic_array(false);
    matrix_hal::MicrophoneCore mic_core(mic_array);

    if (!bus.Init()) return 1;

    int freq = FLAGS_frequency;
    int duration = FLAGS_duration;
    int gain = FLAGS_gain;

    mic_array.Setup(&bus);
    mic_array.SetSamplingRate(freq);
    if (gain > 0) mic_array.SetGain(gain);
    mic_array.ShowConfiguration();
    mic_core.Setup(&bus);

    uint16_t CHANNELS = mic_array.Channels();
    std::vector<std::ofstream> data_to_file(CHANNELS);
    for (uint16_t ch = 0; ch < CHANNELS; ++ch) {
        std::string filename = "BF_mic_" + std::to_string(freq) + "_s16le_channel_" + std::to_string(ch) + ".raw";
        data_to_file[ch].open(filename, std::ios::binary);
        if (!data_to_file[ch]) {
            std::cerr << "Error creando archivo: " << filename << std::endl;
            return 1;
        }
    }

    mqtt_conection_options.set_clean_session(true);
    try {
        client.connect(mqtt_conection_options)->wait();
        std::cout << "Conectado al broker MQTT." << std::endl;
    } catch (const mqtt::exception& exc) {
        std::cerr << "Error al conectar MQTT: " << exc.what() << std::endl;
        return 1;
    }

    SafeQueue<AudioBlock> queue;
    start_threads(&mic_array, queue, data_to_file, freq, duration);

    try {
        client.disconnect()->wait();
        std::cout << "Desconectado del broker MQTT." << std::endl;
    } catch (const mqtt::exception& exc) {
        std::cerr << "Error al desconectar MQTT: " << exc.what() << std::endl;
    }

    for (auto& f : data_to_file) f.close();

    return 0;
}

