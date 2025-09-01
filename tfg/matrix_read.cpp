// FILE    : matrix_read.cpp
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
#include "queue.hpp"
#include <fstream>
#include <cmath>

#define SPEED_OF_SOUND 343.0f // Velocidad del sonido (m/s)
#define MIC_DISTANCE 0.04f    // Distancia entre micrófonos adyacentes (m)

// Configuración de MQTT
const std::string SERVER_IP = "tcp://localhost";
const std::string SERVER_PORT = "1883";
const std::string CLIENT_ID = "AudioPublisher";
const std::string BEAMFORMED_TOPIC = "audio/beamformed";

mqtt::async_client client(SERVER_IP + ":" + SERVER_PORT, CLIENT_ID);
mqtt::connect_options mqtt_connection_options;
std::atomic<bool> running(true);

// Parámetros CLI
DEFINE_int32(frequency, 16000, "Frecuencia de muestreo (Hz)");
DEFINE_int32(duration, 5, "Segundos a grabar (0=continuo)");
DEFINE_int32(gain, 3, "Ganancia del micrófono (dB)");
DEFINE_string(filename, "beamformed_output.wav", "The filename of the beamformed audio");

float normalize_angle(float angle_deg)
{
    while (angle_deg > 180.0f)
        angle_deg -= 360.0f;
    while (angle_deg < -180.0f)
        angle_deg += 360.0f;
    return angle_deg;
}
// Delay-and-Sum con barrido de ángulos + Everloop
void process_beamforming(
    SafeQueue<AudioBlock> &queue,
    uint32_t frequency,
    int duration,
    matrix_hal::Everloop *everloop,
    matrix_hal::EverloopImage *image,
    std::string filename)
{
    const uint16_t num_channels = 8;
    const uint16_t bits_per_sample = 16;
    uint32_t estimated_samples = frequency * duration;
    uint32_t data_size = estimated_samples * bits_per_sample / 8;

    std::ofstream outfile(filename, std::ios::binary);
    if (!outfile.is_open())
    {
        std::cerr << "Error abriendo " << filename << std::endl;
        running = false;
        return;
    }
    write_wav_header(outfile, frequency, bits_per_sample, 1, data_size);

    const float RADIUS = MIC_DISTANCE / (2.0f * sinf(M_PI / num_channels));
    const float ANGLE_MIN = -180.0f, ANGLE_MAX = 180.0f, ANGLE_STEP = 5.0f;
    const int num_leds = image->leds.size();

    while (running)
    {
        AudioBlock block;
        if (!queue.pop(block))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        uint32_t block_size = block.samples[0].size();
        float max_energy = -1.0f, best_angle = 0.0f;
        std::vector<int16_t> best_output(block_size);

        for (float angle_deg = ANGLE_MIN; angle_deg <= ANGLE_MAX; angle_deg += ANGLE_STEP)
        {
            float doa_rad = angle_deg * M_PI / 180.0f;
            std::vector<int32_t> sum(block_size, 0);

            for (uint16_t ch = 0; ch < num_channels; ++ch)
            {
                float mic_angle = 2.0f * M_PI * ch / num_channels;
                float x = RADIUS * cosf(mic_angle);
                float y = RADIUS * sinf(mic_angle);
                float delay_sec = (x * cosf(doa_rad) + y * sinf(doa_rad)) / SPEED_OF_SOUND;
                int delay_samples = static_cast<int>(round(delay_sec * frequency));

                for (uint32_t i = 0; i < block_size; ++i)
                {
                    int idx = static_cast<int>(i) + delay_samples;
                    if (idx >= 0 && idx < static_cast<int>(block_size))
                        sum[i] += block.samples[ch][idx];
                }
            }

            std::vector<int16_t> beamformed(block_size);
            float energy = 0.0f;
            for (uint32_t i = 0; i < block_size; ++i)
            {
                beamformed[i] = sum[i] / num_channels;
                energy += beamformed[i] * beamformed[i];
            }
            if (energy > max_energy)
            {
                max_energy = energy;
                best_output = beamformed;
                best_angle = angle_deg;
            }
        }

        float ANGLE_CORRECTION = 15.0f;
        std::cout << "DOA Calculada: " << normalize_angle(best_angle - ANGLE_CORRECTION) << " grados\n";

        // ——— Everloop: limpia, calcula LED y enciende —
        for (auto &led : image->leds)
            led.red = led.green = led.blue = 0;

        float LED_CORRECTION = -110.0f;
        float angle01 = (normalize_angle(best_angle + LED_CORRECTION) + 180.0f) / 360.0f;
        int pin = static_cast<int>(round(angle01 * (num_leds - 1)));
        image->leds[pin].green = 30;

        everloop->Write(image);

        // ——— Guarda WAV y publica MQTT —
        outfile.write(
            reinterpret_cast<const char *>(best_output.data()),
            best_output.size() * sizeof(int16_t));

        // Old MQTT code
        mqtt::message_ptr pubmsg = mqtt::make_message(
            BEAMFORMED_TOPIC,
            reinterpret_cast<const char *>(best_output.data()),
            best_output.size() * sizeof(int16_t));
        pubmsg->set_qos(1);
        try
        {
            client.publish(pubmsg);
        }
        catch (const mqtt::exception &exc)
        {
            std::cerr << "Error publicando MQTT: " << exc.what() << std::endl;
        }

        /*for (auto &elem : best_output) {
            std::string st = std::to_string(elem);
            mqtt::message_ptr pubmsg = mqtt::make_message(BEAMFORMED_TOPIC, st);
            pubmsg->set_qos(1);
            try {
                client.publish(pubmsg);
            } catch (const mqtt::exception &exc) {
                std::cerr << "Error publicando MQTT: " << exc.what() << std::endl;
            }

        }*/
    }

    outfile.close();
}

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
        "  --filename  : The filename of the beamformed audio\n "
        "                   default: beamformed_output.wav\n"
        "  --gain      : Ganancia del micrófono en dB, 3 para ganancia por defecto (por defecto: 3)\n");

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
        mic_array.SetGain(FLAGS_gain);

    mic_array.ShowConfiguration();
    mic_core.Setup(&bus);

    // Conectar MQTT
    mqtt_connection_options.set_clean_session(true);
    try
    {
        client.connect(mqtt_connection_options)->wait();
        std::cout << "Conectado al broker MQTT." << std::endl;
    }
    catch (const mqtt::exception &exc)
    {
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
        FLAGS_duration);

    // Hilo de beamforming + Everloop
    std::thread processing_thread(
        process_beamforming,
        std::ref(queue),
        FLAGS_frequency,
        FLAGS_duration,
        &everloop,
        &image,
        FLAGS_filename);

    // Esperar hilos
    capture_thread.join();
    running = false;
    processing_thread.join();

    // Desconectar MQTT
    try
    {
        client.disconnect()->wait();
        std::cout << "Desconectado del broker MQTT." << std::endl;
    }
    catch (const mqtt::exception &exc)
    {
        std::cerr << "Error al desconectar MQTT: " << exc.what() << std::endl;
    }

    return 0;
}
