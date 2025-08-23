from typing import Optional

def write_wav_int16(
    data_bytes: bytearray, filename: str = "output.wav", fs: float = 16000
):
    import wave 
    with wave.open(filename, mode="wb") as wav_file:
        wav_file.setnchannels(1)
        wav_file.setsampwidth(2)
        wav_file.setframerate(fs)
        wav_file.writeframes(data_bytes)


def flatten_list(xss: list[any]):
    return [x for xs in xss for x in xs]


def list_bytes_to_bytearray(list_bytes: list[bytes]) -> bytearray:
    return bytearray(b"".join(list_bytes))


def bytearray_to_int16_list(byarray: bytearray) -> list[int]:
    import sys

    hex_repl = byarray.hex()
    list_int16_t: list[int] = []
    for char_idx in range(0, len(hex_repl), 4):
        # A hex digit represents 4 contiguous bits. As a int16_t is 16 bits long it has 4 hex digits.
        hex_data = hex_repl[char_idx : char_idx + 4]
        bytes_rep = bytes.fromhex(hex_data)
        int16_t = int.from_bytes(bytes_rep, byteorder=sys.byteorder, signed=True)
        list_int16_t.append(int16_t)

        # For debugging
        # A memory address is in bytes, and each byte has two hex numbers.
        # mem_addr_data = char_idx // 2  # Useful for debugging if we only save bytes
        # mem_addr_data_wav = mem_addr_data + 0x2C  # Sum len(wav_header) if using wav.
        # print(f"{char_idx=} {hex(mem_addr)=}, {int16_t=}, {hex_data=}")

    return list_int16_t


def int16_list_to_bytearray(list_int16t: list[int]) -> bytearray:
    import sys

    list_bytes: list[bytes] = []
    for num in list_int16t:
        if num < -32768 or num > 32767:
            raise RuntimeError(f"The number {num} is not an int16_t")
        nb = num.to_bytes(length=2, byteorder=sys.byteorder, signed=True)
        list_bytes.append(nb)
    return bytearray(b"".join(list_bytes))

def setup_microphone(fs: Optional[int] = None, gain: Optional[int] = None):
    import matrix_pybind_bindings as hal
    if fs is None and gain is None:
        return hal.microphone(16000, 0)
    if fs is None:
        return hal.microphone(16000, gain)
    if gain is None:
        return hal.microphone(fs, 0)
    return hal.microphone(fs, gain)
