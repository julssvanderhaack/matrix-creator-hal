# FILE : script_python.py
# AUTOR: Julio Albisua
# INFO:  Script Python para recibir audio beamformed a través de MQTT.

import paho.mqtt.client as mqtt
import numpy as np
import sounddevice as sd
import threading
import time
from collections import deque
import scipy.io.wavfile

# === Parámetros de usuario ===
BROKER = "192.168.1.128"
PORT = 1883
TOPIC = "audio/beamformed"
FS = 16000  # Frecuencia de muestreo en Hz
BUFFER_DURATION = 5  # Duración del buffer en segundos
RUN_DURATION = 30  # Tiempo total de ejecución en segundos

# === Buffer y control ===
BUFFER_SIZE = FS * BUFFER_DURATION
audio_buffer = deque(maxlen=BUFFER_SIZE)
buffer_lock = threading.Lock()
buffer_ready = threading.Condition(lock=buffer_lock)
running = True

# Buffer para guardar todas las muestras recibidas
all_samples = []

# === Callback MQTT: Conexión ===
def on_connect(client, userdata, flags, rc):
    print("Conectado al broker con código de resultado", rc)
    client.subscribe(TOPIC)

# === Callback MQTT: Mensaje recibido ===
def on_message(client, userdata, msg):
    try:
        samples_int16 = np.frombuffer(msg.payload, dtype=np.int16)
        if samples_int16.size == 0:
            print("⚠️ Bloque vacío recibido, no se reproduce.")
            return

        audio = samples_int16.astype(np.float32) / 32768.0

        with buffer_ready:
            audio_buffer.extend(audio)
            buffer_ready.notify()  # Despierta el hilo de reproducción

        # Guarda todas las muestras originales (int16)
        all_samples.extend(samples_int16)

        print(f"{len(audio)} muestras añadidas al buffer")

    except Exception as e:
        print(f"Error al procesar mensaje: {e}")

# === Hilo de reproducción en tiempo real ===
def playback_loop():
    global running
    CHUNK_SIZE = 1024

    while running:
        with buffer_ready:
            while len(audio_buffer) < CHUNK_SIZE and running:
                buffer_ready.wait()  # Espera hasta que haya suficientes muestras o finalice

            if not running:
                break

            chunk = [audio_buffer.popleft() for _ in range(CHUNK_SIZE)]

        sd.play(np.array(chunk, dtype=np.float32), samplerate=FS, blocking=True)

# === Función principal ===
def main():
    global running

    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(BROKER, PORT, 60)

    # Hilo de reproducción
    player_thread = threading.Thread(target=playback_loop)
    player_thread.start()

    client.loop_start()
    print(f"Ejecutando durante {RUN_DURATION} segundos...")

    time.sleep(RUN_DURATION)

    running = False
    with buffer_ready:
        buffer_ready.notify_all()  # Despierta el hilo si está esperando

    client.loop_stop()
    client.disconnect()
    player_thread.join()

    print("Conexión cerrada y script finalizado.")

    # Guardar el audio en un archivo WAV con cabecera
    if all_samples:
        print(f"Guardando {len(all_samples)} muestras en 'muestras.wav'...")
        scipy.io.wavfile.write("muestras.wav", FS, np.array(all_samples, dtype=np.int16))
        print("Archivo 'muestras.wav' guardado correctamente.")

# === Ejecutar ===
if __name__ == "__main__":
    main()
# === Fin del script ===