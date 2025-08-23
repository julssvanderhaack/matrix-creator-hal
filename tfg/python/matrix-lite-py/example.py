import wave
import matrix_lite as m
import time


mic = m.microphone

print("* recording")
mic.start_async()
time.sleep(10)
mic.stop_async()
print("* done recording")

# read & store microphone data per frame read
framesch1 = []
while y := mic.read_async():
    fr = y[0]
    framesch1.append(fr)

frames_int = m.mic_helpers.flatten_list(framesch1)


# Convert all the read data to bytes from int16
data_b = m.mic_helpers.int16_list_to_bytearray(frames_int)
m.mic_helpers.write_wav_int16(data_b)

