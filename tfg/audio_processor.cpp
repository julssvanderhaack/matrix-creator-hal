#include "audio_processor.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <fftw3.h>
#include <cmath>

#define SPEED_OF_SOUND 343.0f      // Velocidad del sonido (m/s)
#define MIC_DISTANCE 0.04f         // Distancia típica entre micrófonos (m)
#define FREQ_CENTER 2000.0f        // Frecuencia central para beamforming narrowband (Hz)

// Función para escribir el encabezado WAV inicial
void write_wav_header(std::ofstream &out, uint32_t sample_rate, uint16_t bits_per_sample, uint16_t num_channels, uint32_t data_size) {
    uint32_t byte_rate = sample_rate * num_channels * bits_per_sample / 8;
    uint16_t block_align = num_channels * bits_per_sample / 8;
    uint32_t chunk_size = 36 + data_size;

    out.seekp(0, std::ios::beg);
    out.write("RIFF", 4);
    out.write(reinterpret_cast<const char*>(&chunk_size), 4);
    out.write("WAVE", 4);

    out.write("fmt ", 4);
    uint32_t subchunk1_size = 16;
    uint16_t audio_format = 1; // PCM
    out.write(reinterpret_cast<const char*>(&subchunk1_size), 4);
    out.write(reinterpret_cast<const char*>(&audio_format), 2);
    out.write(reinterpret_cast<const char*>(&num_channels), 2);
    out.write(reinterpret_cast<const char*>(&sample_rate), 4);
    out.write(reinterpret_cast<const char*>(&byte_rate), 4);
    out.write(reinterpret_cast<const char*>(&block_align), 2);
    out.write(reinterpret_cast<const char*>(&bits_per_sample), 2);

    out.write("data", 4);
    out.write(reinterpret_cast<const char*>(&data_size), 4);
}

// Hilo de captura: lee bloques de audio multicanal y los coloca en la cola
void capture_audio(matrix_hal::MicrophoneArray* mic_array,
                   SafeQueue<AudioBlock>& queue,
                   int duration) {
    const uint32_t BLOCK_SIZE = mic_array->NumberOfSamples();
    const uint16_t CHANNELS = mic_array->Channels();

    auto start_time = std::chrono::steady_clock::now();
    auto end_time = start_time + std::chrono::seconds(duration);

    while (running && (duration == 0 || std::chrono::steady_clock::now() < end_time)) {
        mic_array->Read();
        AudioBlock block;
        block.samples.resize(CHANNELS, std::vector<int16_t>(BLOCK_SIZE));
        for (uint32_t s = 0; s < BLOCK_SIZE; ++s)
            for (uint16_t ch = 0; ch < CHANNELS; ++ch)
                block.samples[ch][s] = mic_array->At(s, ch);

        queue.push(block);
    }
    running = false;
}

// Hilo de procesamiento: hace beamforming de banda estrecha y escribe WAV + MQTT
void process_beamforming(SafeQueue<AudioBlock>& queue, uint32_t frequency, int duration) {
    const uint16_t num_channels = 8;              // Número de micrófonos en tu array
    const uint16_t bits_per_sample = 16;
    uint32_t estimated_samples = frequency * duration;
    uint32_t data_size = estimated_samples * bits_per_sample / 8;
    uint32_t BLOCK_SIZE = 512;                    // Tamaño del bloque; ajusta si es necesario

    std::ofstream outfile("beamformed_output.wav", std::ios::binary);
    if (!outfile.is_open()) {
        std::cerr << "Error abriendo beamformed_output.wav" << std::endl;
        running = false;
        return;
    }
    write_wav_header(outfile, frequency, bits_per_sample, 1, data_size);

    // Buffers y planes de FFTW para GCC-PHAT
    fftwf_plan plan_forward, plan_backward;
    float *in1 = fftwf_alloc_real(BLOCK_SIZE);
    float *in2 = fftwf_alloc_real(BLOCK_SIZE);
    fftwf_complex *fft1 = fftwf_alloc_complex(BLOCK_SIZE/2+1);
    fftwf_complex *fft2 = fftwf_alloc_complex(BLOCK_SIZE/2+1);
    fftwf_complex *cross_spectrum = fftwf_alloc_complex(BLOCK_SIZE/2+1);
    float *gcc_phat = fftwf_alloc_real(BLOCK_SIZE);

    plan_forward = fftwf_plan_dft_r2c_1d(BLOCK_SIZE, in1, fft1, FFTW_ESTIMATE);
    plan_backward = fftwf_plan_dft_c2r_1d(BLOCK_SIZE, cross_spectrum, gcc_phat, FFTW_ESTIMATE);

    while (running) {
        AudioBlock block;
        if (queue.pop(block)) {
            uint32_t block_size = block.samples[0].size();
            float doa_accum = 0.0f;
            int doa_count = 0;

            // Calcular DOA estimando TDOA entre pares de micrófonos consecutivos
            for (uint16_t ch1 = 0; ch1 < num_channels; ++ch1) {
                uint16_t ch2 = (ch1 + 1) % num_channels; // siguiente micrófono

                for (uint32_t i = 0; i < block_size; ++i) {
                    in1[i] = static_cast<float>(block.samples[ch1][i]);
                    in2[i] = static_cast<float>(block.samples[ch2][i]);
                }

                fftwf_execute_dft_r2c(plan_forward, in1, fft1);
                fftwf_execute_dft_r2c(plan_forward, in2, fft2);

                for (uint32_t k = 0; k < BLOCK_SIZE/2+1; ++k) {
                    float re = fft1[k][0]*fft2[k][0] + fft1[k][1]*fft2[k][1];
                    float im = fft1[k][1]*fft2[k][0] - fft1[k][0]*fft2[k][1];
                    float mag = sqrtf(re*re + im*im) + 1e-8f;
                    cross_spectrum[k][0] = re / mag;
                    cross_spectrum[k][1] = im / mag;
                }

                fftwf_execute_dft_c2r(plan_backward, cross_spectrum, gcc_phat);

                // Buscar pico en GCC-PHAT para estimar TDOA
                uint32_t max_idx = 0;
                float max_val = -1e9;
                for (uint32_t i = 0; i < block_size; ++i) {
                    if (gcc_phat[i] > max_val) {
                        max_val = gcc_phat[i];
                        max_idx = i;
                    }
                }
                int tdoa_samples = max_idx < block_size/2 ? max_idx : max_idx - block_size;
                float tdoa_sec = static_cast<float>(tdoa_samples) / frequency;

                // Convertir TDOA a DOA según la ecuación del artículo
                float arg = SPEED_OF_SOUND * tdoa_sec / MIC_DISTANCE;
                if (fabsf(arg) <= 1.0f) {
                    float doa = asinf(arg) * 180.0f / M_PI;  // DOA en grados
                    doa_accum += doa;
                    doa_count++;
                }
            }

            float doa_avg = doa_accum / doa_count;
            std::cout << "DOA estimada: " << doa_avg << " grados" << std::endl;

            // Aplicar delay-and-sum narrowband en frecuencia central
            std::vector<int32_t> sum(block_size, 0);
            float omega = 2 * M_PI * FREQ_CENTER;

            for (uint16_t ch = 0; ch < num_channels; ++ch) {
                float angle_mic = (2 * M_PI * ch) / num_channels;
                float mic_pos_x = MIC_DISTANCE * cosf(angle_mic);
                float mic_pos_y = MIC_DISTANCE * sinf(angle_mic);
                float doa_rad = doa_avg * M_PI / 180.0f;
                float delay = (mic_pos_x * cosf(doa_rad) + mic_pos_y * sinf(doa_rad)) / SPEED_OF_SOUND;
                float phase_shift = omega * delay;

                for (uint32_t i = 0; i < block_size; ++i) {
                    float shifted = block.samples[ch][i] * cosf(phase_shift);  // aproximación en banda estrecha
                    sum[i] += static_cast<int32_t>(shifted);
                }
            }

            std::vector<int16_t> beamformed(block_size);
            for (uint32_t i = 0; i < block_size; ++i) {
                beamformed[i] = sum[i] / num_channels;
            }

            outfile.write(reinterpret_cast<const char*>(beamformed.data()), beamformed.size() * sizeof(int16_t));

            mqtt::message_ptr pubmsg = mqtt::make_message(
                BEAMFORMED_TOPIC,
                reinterpret_cast<const char*>(beamformed.data()),
                beamformed.size() * sizeof(int16_t));
            pubmsg->set_qos(1);
            try {
                client.publish(pubmsg);
            } catch (const mqtt::exception &exc) {
                std::cerr << "Error publicando MQTT: " << exc.what() << std::endl;
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    outfile.close();
    fftwf_destroy_plan(plan_forward);
    fftwf_destroy_plan(plan_backward);
    fftwf_free(in1); fftwf_free(in2);
    fftwf_free(fft1); fftwf_free(fft2); fftwf_free(cross_spectrum); fftwf_free(gcc_phat);
}