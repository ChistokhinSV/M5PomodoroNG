#include "AudioPlayer.h"
#include <Arduino.h>

// Sound file path mapping
constexpr const char* AudioPlayer::SOUND_PATHS[];

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

    // Set initial volume (M5.Speaker expects 0-255)
    setVolumeInternal(map(current_volume, 0, 100, 0, 255));

    Serial.println("[AudioPlayer] Initialized");
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

    // Try to play WAV file
    const char* path = SOUND_PATHS[static_cast<int>(sound)];
    if (!playWavFile(path)) {
        // Fallback to beep if WAV file not found
        Serial.printf("[AudioPlayer] WAV file not found: %s, using beep\n", path);
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

void AudioPlayer::update() {
    // Update playing state
    playing = isPlaying();
}

// Private methods

bool AudioPlayer::playWavFile(const char* path) {
    // Check if file exists in SPIFFS
    // Note: This is a placeholder - actual WAV playback requires:
    // 1. SPIFFS initialization
    // 2. WAV file parsing
    // 3. I2S audio streaming
    //
    // For MVP, we'll use M5.Speaker.playWav() if available,
    // otherwise fall back to tones.

    // Attempt to play using M5Unified's built-in method
    // (availability depends on M5Unified version)
    #ifdef M5_SPEAKER_PLAY_WAV
    if (M5.Speaker.playWav(path)) {
        Serial.printf("[AudioPlayer] Playing WAV: %s\n", path);
        playing = true;
        return true;
    }
    #endif

    // WAV playback not available or file not found
    return false;
}

void AudioPlayer::setVolumeInternal(uint8_t volume_255) {
    // M5.Speaker.setVolume() expects 0-255
    M5.Speaker.setVolume(volume_255);
}
