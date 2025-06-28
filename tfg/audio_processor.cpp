#include "audio_processor.hpp"
#include <iostream>
#include <thread>
#include <chrono>

void capture_audio(matrix_hal::MicrophoneArray* mic_array,
                   SafeQueue<AudioBlock>& queue,
                   uint32_t frequency,
                   int duration) {
    const uint32_t BLOCK_SIZE = mic_array->NumberOfSamples();
    const uint16_t CHANNELS = mic_array->Channels();
    uint32_t total_samples = 0;
    uint32_t total_needed = frequency * duration;

    while (running && total_samples < total_needed) {
        mic_array->Read();
        AudioBlock block;
        block.samples.resize(CHANNELS, std::vector<int16_t>(BLOCK_SIZE));
        for (uint32_t s = 0; s < BLOCK_SIZE; ++s)
            for (uint16_t ch = 0; ch < CHANNELS; ++ch)
                block.samples[ch][s] = mic_array->At(s, ch);

        queue.push(block);
        total_samples += BLOCK_SIZE;
    }

    running = false;
}

void process_audio(SafeQueue<AudioBlock>& queue,
                   std::vector<std::ofstream>& data_to_file,
                   uint16_t CHANNELS) {
    while (running) {
        AudioBlock block;
        if (queue.pop(block)) {
            uint32_t block_size = block.samples[0].size();
            for (uint16_t ch = 0; ch < CHANNELS; ++ch) {
                data_to_file[ch].write(
                    reinterpret_cast<const char*>(block.samples[ch].data()),
                    block_size * sizeof(int16_t));
                data_to_file[ch].flush();

                std::string topic = TOPIC_BASE + std::to_string(ch);
                auto pubmsg = mqtt::make_message(
                    topic,
                    reinterpret_cast<const char*>(block.samples[ch].data()),
                    block_size * sizeof(int16_t));
                pubmsg->set_qos(1);
                try {
                    client.publish(pubmsg);
                } catch (const mqtt::exception &exc) {
                    std::cerr << "Error publicando MQTT: " << exc.what() << std::endl;
                }
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

