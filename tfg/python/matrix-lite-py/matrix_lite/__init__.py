import matrix_pybind_bindings as hal

__name__ = "matrix_lite"
__all__ = ["led", "sensors", "gpio", "info", "microphone", "mic_helper"]

# Exported variables
gpio = hal.gpio()
info = hal.info()
import matrix_lite.led as led
import matrix_lite.sensors as sensors

# The microphones are hardcoded to fs=16k and gain=0, to parametrize import it as
# `import matrix_pybind_bindings as m` and instanciate one (`x = m.microphone(fs, gain)`)

from matrix_lite.mic_helpers import write_wav_int16
from matrix_lite.mic_helpers import flatten_list
from matrix_lite.mic_helpers import list_bytes_to_bytearray
from matrix_lite.mic_helpers import bytearray_to_int16_list
from matrix_lite.mic_helpers import int16_list_to_bytearray
from matrix_lite.mic_helpers import setup_microphone

microphone = setup_microphone(16000, 0) 
