#!/usr/bin/env python3
"""
WAV to C Array Converter
Converts WAV audio files to C byte arrays for embedding in firmware (PROGMEM)

Usage:
    python wav2array.py input.wav variable_name

Example:
    python wav2array.py work_start.wav wav_work_start > audio_data.cpp
"""

import sys
import os
import struct


def read_wav_header(data):
    """Parse WAV file header and return format info"""
    if data[0:4] != b'RIFF':
        raise ValueError("Not a valid WAV file (missing RIFF header)")

    if data[8:12] != b'WAVE':
        raise ValueError("Not a valid WAV file (missing WAVE marker)")

    # Find fmt chunk
    pos = 12
    while pos < len(data) - 8:
        chunk_id = data[pos:pos+4]
        chunk_size = struct.unpack('<I', data[pos+4:pos+8])[0]

        if chunk_id == b'fmt ':
            # Parse format chunk
            audio_format = struct.unpack('<H', data[pos+8:pos+10])[0]
            num_channels = struct.unpack('<H', data[pos+10:pos+12])[0]
            sample_rate = struct.unpack('<I', data[pos+12:pos+16])[0]
            bits_per_sample = struct.unpack('<H', data[pos+22:pos+24])[0]

            return {
                'format': audio_format,
                'channels': num_channels,
                'sample_rate': sample_rate,
                'bits_per_sample': bits_per_sample,
                'chunk_size': chunk_size
            }

        pos += 8 + chunk_size
        # Align to word boundary
        if chunk_size % 2 == 1:
            pos += 1

    raise ValueError("No fmt chunk found in WAV file")


def wav_to_c_array(wav_file, var_name):
    """Convert WAV file to C array format"""
    with open(wav_file, 'rb') as f:
        data = f.read()

    # Validate and parse header
    header_info = read_wav_header(data)

    # Print file info as comment
    output = f"// {os.path.basename(wav_file)}\n"
    output += f"// Format: {header_info['channels']} channel(s), "
    output += f"{header_info['sample_rate']} Hz, "
    output += f"{header_info['bits_per_sample']}-bit\n"
    output += f"// Size: {len(data)} bytes\n\n"

    # Generate C array
    output += f"const uint8_t PROGMEM {var_name}[] = {{\n"

    # Write bytes in rows of 16
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        hex_bytes = ", ".join(f"0x{b:02X}" for b in chunk)
        output += f"    {hex_bytes}"
        if i + 16 < len(data):
            output += ","
        output += "\n"

    output += "};\n"
    output += f"const uint32_t {var_name}_len = sizeof({var_name});\n"

    return output


def main():
    if len(sys.argv) != 3:
        print("Usage: python wav2array.py <input.wav> <variable_name>", file=sys.stderr)
        print("\nExample:", file=sys.stderr)
        print("  python wav2array.py work_start.wav wav_work_start", file=sys.stderr)
        sys.exit(1)

    wav_file = sys.argv[1]
    var_name = sys.argv[2]

    if not os.path.exists(wav_file):
        print(f"Error: File '{wav_file}' not found", file=sys.stderr)
        sys.exit(1)

    if not wav_file.lower().endswith('.wav'):
        print("Warning: File doesn't have .wav extension", file=sys.stderr)

    try:
        result = wav_to_c_array(wav_file, var_name)
        print(result)
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
