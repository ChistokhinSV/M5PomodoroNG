#!/usr/bin/env python3
"""
Generate simple WAV audio files for Pomodoro timer
Creates 16kHz, 16-bit mono WAV files with tone sequences

Usage:
    python generate_sounds.py
"""

import struct
import math
import os


def generate_tone(frequency, duration_ms, sample_rate=16000, amplitude=0.3):
    """Generate a sine wave tone

    Args:
        frequency: Tone frequency in Hz
        duration_ms: Duration in milliseconds
        sample_rate: Sample rate in Hz (default 16000)
        amplitude: Amplitude 0.0-1.0 (default 0.3)

    Returns:
        List of 16-bit signed PCM samples
    """
    num_samples = int(sample_rate * duration_ms / 1000)
    samples = []

    for i in range(num_samples):
        t = i / sample_rate
        value = amplitude * math.sin(2 * math.pi * frequency * t)
        # Convert to 16-bit signed integer
        sample = int(value * 32767)
        samples.append(sample)

    return samples


def generate_silence(duration_ms, sample_rate=16000):
    """Generate silence (zeros)"""
    num_samples = int(sample_rate * duration_ms / 1000)
    return [0] * num_samples


def write_wav(filename, samples, sample_rate=16000):
    """Write 16-bit mono WAV file

    Args:
        filename: Output filename
        samples: List of 16-bit signed integer samples
        sample_rate: Sample rate in Hz
    """
    num_samples = len(samples)
    bits_per_sample = 16
    num_channels = 1
    byte_rate = sample_rate * num_channels * bits_per_sample // 8
    block_align = num_channels * bits_per_sample // 8
    data_size = num_samples * block_align

    with open(filename, 'wb') as f:
        # RIFF header
        f.write(b'RIFF')
        f.write(struct.pack('<I', 36 + data_size))  # File size - 8
        f.write(b'WAVE')

        # fmt chunk
        f.write(b'fmt ')
        f.write(struct.pack('<I', 16))  # Chunk size
        f.write(struct.pack('<H', 1))   # Audio format (1 = PCM)
        f.write(struct.pack('<H', num_channels))
        f.write(struct.pack('<I', sample_rate))
        f.write(struct.pack('<I', byte_rate))
        f.write(struct.pack('<H', block_align))
        f.write(struct.pack('<H', bits_per_sample))

        # data chunk
        f.write(b'data')
        f.write(struct.pack('<I', data_size))

        # Sample data
        for sample in samples:
            f.write(struct.pack('<h', sample))

    file_size = os.path.getsize(filename)
    print(f"Generated: {filename} ({file_size} bytes, {num_samples} samples, {num_samples/sample_rate:.2f}s)")


def generate_work_start():
    """Work session start: ascending 3-tone sequence"""
    samples = []
    samples += generate_tone(800, 150)   # 800 Hz for 150ms
    samples += generate_silence(50)
    samples += generate_tone(1000, 150)  # 1000 Hz for 150ms
    samples += generate_silence(50)
    samples += generate_tone(1200, 200)  # 1200 Hz for 200ms
    return samples


def generate_rest_start():
    """Break start: descending 2-tone sequence"""
    samples = []
    samples += generate_tone(1000, 150)  # 1000 Hz for 150ms
    samples += generate_silence(50)
    samples += generate_tone(600, 200)   # 600 Hz for 200ms
    return samples


def generate_long_rest_start():
    """Long break start: descending 3-tone sequence"""
    samples = []
    samples += generate_tone(1200, 150)  # 1200 Hz for 150ms
    samples += generate_silence(50)
    samples += generate_tone(800, 150)   # 800 Hz for 150ms
    samples += generate_silence(50)
    samples += generate_tone(500, 300)   # 500 Hz for 300ms
    return samples


def generate_warning():
    """Warning sound: double beep"""
    samples = []
    samples += generate_tone(1500, 100)  # 1500 Hz for 100ms
    samples += generate_silence(100)
    samples += generate_tone(1500, 100)  # 1500 Hz for 100ms
    return samples


def main():
    """Generate all audio files"""
    # Create output directory
    output_dir = os.path.join(os.path.dirname(__file__), '..', 'data', 'audio')
    os.makedirs(output_dir, exist_ok=True)

    print("Generating audio files (16kHz, 16-bit mono WAV)...\n")

    # Generate each sound
    sounds = {
        'work_start.wav': generate_work_start(),
        'rest_start.wav': generate_rest_start(),
        'long_rest_start.wav': generate_long_rest_start(),
        'warning.wav': generate_warning()
    }

    for filename, samples in sounds.items():
        filepath = os.path.join(output_dir, filename)
        write_wav(filepath, samples)

    print(f"\nAll files saved to: {output_dir}")
    print("\nNext step: Convert to C arrays using:")
    print("  cd tools")
    print("  python wav2array.py ../data/audio/work_start.wav wav_work_start > ../src/hardware/audio_data_part1.txt")
    print("  python wav2array.py ../data/audio/rest_start.wav wav_rest_start > ../src/hardware/audio_data_part2.txt")
    print("  python wav2array.py ../data/audio/long_rest_start.wav wav_long_rest_start > ../src/hardware/audio_data_part3.txt")
    print("  python wav2array.py ../data/audio/warning.wav wav_warning > ../src/hardware/audio_data_part4.txt")
    print("\nThen combine all parts into audio_data.cpp")


if __name__ == '__main__':
    main()
