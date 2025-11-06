#include "AudioPlayer.h"
#include <Arduino.h>

// Include embedded audio data
#include "audio_data.cpp"

AudioPlayer::AudioPlayer()
    : current_volume(70),
      muted(false),
      playing(false) {
}

bool AudioPlayer::begin() {
    // M5Unified already initialized M5.Speaker
    if (!M5.Speaker.isEnabled()) {
        Serial.println("[AudioPlayer] WARNING: Speaker not available");
        return false;
    }

    // Speaker is already enabled by M5.begin() in M5Unified
    // No need to manually enable AXP192 - M5Unified handles it

    // Set initial volume (M5.Speaker expects 0-255)
    setVolumeInternal(map(current_volume, 0, 100, 0, 255));

    Serial.println("[AudioPlayer] Initialized");
    Serial.printf("[AudioPlayer] Speaker enabled: %s\n", M5.Speaker.isEnabled() ? "yes" : "no");
    Serial.printf("[AudioPlayer] Available channels: 8\n");
    return true;
}

void AudioPlayer::play(Sound sound) {
    if (muted) {
        Serial.println("[AudioPlayer] Muted, skipping playback");
        return;
    }

    // Stop any currently playing sound
    stop();

    // Play generated tones for BEEP sounds
    if (sound == Sound::BEEP_SHORT) {
        playTone(1000, 200);  // 1kHz, 200ms
        return;
    } else if (sound == Sound::BEEP_LONG) {
        playTone(1000, 500);  // 1kHz, 500ms
        return;
    }

    // Map Sound enum to PROGMEM WAV data
    const uint8_t* wav_data = nullptr;
    size_t wav_len = 0;

    switch (sound) {
        case Sound::WORK_START:
            wav_data = wav_work_start;
            wav_len = wav_work_start_len;
            break;
        case Sound::REST_START:
            wav_data = wav_rest_start;
            wav_len = wav_rest_start_len;
            break;
        case Sound::LONG_REST_START:
            wav_data = wav_long_rest_start;
            wav_len = wav_long_rest_start_len;
            break;
        case Sound::WARNING:
            wav_data = wav_warning;
            wav_len = wav_warning_len;
            break;
        default:
            Serial.println("[AudioPlayer] Unknown sound, using beep");
            playBeep();
            return;
    }

    // Try to play WAV from PROGMEM
    if (!playWavFile(wav_data, wav_len)) {
        // Fallback to beep if playback fails
        Serial.println("[AudioPlayer] WAV playback failed, using beep");
        playBeep();
    }
}

void AudioPlayer::stop() {
    if (playing) {
        M5.Speaker.stop();
        playing = false;
        Serial.println("[AudioPlayer] Stopped playback");
    }
}

bool AudioPlayer::isPlaying() const {
    // M5.Speaker provides isPlaying() method
    return M5.Speaker.isPlaying();
}

void AudioPlayer::setVolume(uint8_t percent) {
    current_volume = constrain(percent, 0, 100);

    if (!muted) {
        setVolumeInternal(map(current_volume, 0, 100, 0, 255));
    }

    Serial.printf("[AudioPlayer] Volume set to %d%%\n", current_volume);
}

void AudioPlayer::mute() {
    muted = true;
    setVolumeInternal(0);
    Serial.println("[AudioPlayer] Muted");
}

void AudioPlayer::unmute() {
    muted = false;
    setVolumeInternal(map(current_volume, 0, 100, 0, 255));
    Serial.println("[AudioPlayer] Unmuted");
}

void AudioPlayer::playTone(uint16_t frequency_hz, uint16_t duration_ms) {
    if (muted) return;

    Serial.printf("[AudioPlayer] Playing tone: %u Hz, %u ms\n", frequency_hz, duration_ms);

    // M5.Speaker.tone() plays a generated tone
    M5.Speaker.tone(frequency_hz, duration_ms);
    playing = true;
}

void AudioPlayer::playBeep() {
    playTone(1000, 100);  // 1kHz, 100ms beep
}

void AudioPlayer::playWarning() {
    if (!muted) {
        play(Sound::WARNING);
    }
}

void AudioPlayer::update() {
    // Update playing state
    playing = isPlaying();
}

// Private methods

bool AudioPlayer::playWavFile(const uint8_t* wav_data, size_t len) {
    if (wav_data == nullptr || len == 0) {
        Serial.println("[AudioPlayer] Invalid WAV data");
        return false;
    }

    // Play WAV from PROGMEM using M5Unified Speaker API
    // M5.Speaker.playWav() accepts const uint8_t* and works with PROGMEM
    // Auto-assign to any available channel (-1), no repeat (1), no loop (false)
    if (M5.Speaker.playWav(wav_data, len, 1, -1, false)) {
        Serial.printf("[AudioPlayer] Playing WAV from PROGMEM (%d bytes)\n", len);
        playing = true;
        return true;
    }

    // WAV playback failed
    Serial.println("[AudioPlayer] M5.Speaker.playWav() failed");
    return false;
}

void AudioPlayer::setVolumeInternal(uint8_t volume_255) {
    // M5.Speaker.setVolume() expects 0-255
    M5.Speaker.setVolume(volume_255);
}
