#include "AudioPlayer.h"
#include <Arduino.h>

// Include embedded audio data
#include "audio_data.cpp"

AudioPlayer::AudioPlayer()
    : current_volume(70),
      muted(false),
      playing(false),
      sd_manager(nullptr),
      current_source(AudioSource::FLASH),
      sd_audio_loaded(false),
      sd_wav_work_start(nullptr),
      sd_wav_work_start_len(0),
      sd_wav_rest_start(nullptr),
      sd_wav_rest_start_len(0),
      sd_wav_long_rest_start(nullptr),
      sd_wav_long_rest_start_len(0),
      sd_wav_warning(nullptr),
      sd_wav_warning_len(0) {
}

// Destructor - free SD buffers
AudioPlayer::~AudioPlayer() {
    freeSDBuffers();
}

bool AudioPlayer::begin() {
    // Backward compatibility - use FLASH only
    return begin(nullptr, AudioSource::FLASH);
}

bool AudioPlayer::begin(SDManager* sd_manager, AudioSource source) {
    // M5Unified already initialized M5.Speaker
    if (!M5.Speaker.isEnabled()) {
        Serial.println("[AudioPlayer] WARNING: Speaker not available");
        return false;
    }

    // Store SD manager reference
    this->sd_manager = sd_manager;

    // Set initial volume (M5.Speaker expects 0-255)
    setVolumeInternal(map(current_volume, 0, 100, 0, 255));

    Serial.println("[AudioPlayer] Initialized");
    Serial.printf("[AudioPlayer] Speaker enabled: %s\n", M5.Speaker.isEnabled() ? "yes" : "no");

    // Try to load audio from SD card if requested
    if (source == AudioSource::SD_CARD || source == AudioSource::AUTO) {
        if (sd_manager && sd_manager->isMounted()) {
            Serial.println("[AudioPlayer] Attempting to load audio from SD card...");
            if (loadAudioFromSD()) {
                current_source = AudioSource::SD_CARD;
                sd_audio_loaded = true;
                Serial.println("[AudioPlayer] SD card audio loaded successfully");
            } else {
                Serial.println("[AudioPlayer] SD audio load failed");
                if (source == AudioSource::SD_CARD) {
                    // User explicitly requested SD only - fail
                    Serial.println("[AudioPlayer] ERROR: SD_CARD mode but audio not available");
                    return false;
                } else {
                    // AUTO mode - fallback to FLASH
                    Serial.println("[AudioPlayer] Falling back to FLASH audio");
                    current_source = AudioSource::FLASH;
                }
            }
        } else {
            Serial.println("[AudioPlayer] SD card not mounted, using FLASH audio");
            current_source = AudioSource::FLASH;
        }
    } else {
        // FLASH explicitly requested
        current_source = AudioSource::FLASH;
        Serial.println("[AudioPlayer] Using FLASH audio (embedded)");
    }

    Serial.printf("[AudioPlayer] Audio source: %s\n",
                  current_source == AudioSource::SD_CARD ? "SD_CARD" : "FLASH");

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

    // Map Sound enum to WAV data (SD or PROGMEM)
    const uint8_t* wav_data = nullptr;
    size_t wav_len = 0;

    // Try SD card first if available
    if (sd_audio_loaded) {
        switch (sound) {
            case Sound::WORK_START:
                wav_data = sd_wav_work_start;
                wav_len = sd_wav_work_start_len;
                break;
            case Sound::REST_START:
                wav_data = sd_wav_rest_start;
                wav_len = sd_wav_rest_start_len;
                break;
            case Sound::LONG_REST_START:
                wav_data = sd_wav_long_rest_start;
                wav_len = sd_wav_long_rest_start_len;
                break;
            case Sound::WARNING:
                wav_data = sd_wav_warning;
                wav_len = sd_wav_warning_len;
                break;
            default:
                break;
        }
    }

    // Fallback to PROGMEM if SD not available or failed
    if (wav_data == nullptr) {
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

// SD card audio loading (MP-71)

bool AudioPlayer::loadAudioFromSD() {
    if (!sd_manager || !sd_manager->isMounted()) {
        return false;
    }

    // Free any existing buffers
    freeSDBuffers();

    // Load all 4 WAV files
    bool success = true;

    if (!loadWavFromSD("/audio/work_start.wav", &sd_wav_work_start, &sd_wav_work_start_len)) {
        Serial.println("[AudioPlayer] Failed to load work_start.wav");
        success = false;
    }

    if (!loadWavFromSD("/audio/rest_start.wav", &sd_wav_rest_start, &sd_wav_rest_start_len)) {
        Serial.println("[AudioPlayer] Failed to load rest_start.wav");
        success = false;
    }

    if (!loadWavFromSD("/audio/long_rest_start.wav", &sd_wav_long_rest_start, &sd_wav_long_rest_start_len)) {
        Serial.println("[AudioPlayer] Failed to load long_rest_start.wav");
        success = false;
    }

    if (!loadWavFromSD("/audio/warning.wav", &sd_wav_warning, &sd_wav_warning_len)) {
        Serial.println("[AudioPlayer] Failed to load warning.wav");
        success = false;
    }

    if (!success) {
        // If any file failed, free all buffers and report failure
        freeSDBuffers();
        return false;
    }

    Serial.printf("[AudioPlayer] Loaded %d audio files from SD (total: %d bytes)\n",
                  4, sd_wav_work_start_len + sd_wav_rest_start_len +
                     sd_wav_long_rest_start_len + sd_wav_warning_len);

    return true;
}

bool AudioPlayer::loadWavFromSD(const char* path, uint8_t** buffer, size_t* len) {
    if (!sd_manager || !buffer || !len) {
        return false;
    }

    // Check if file exists
    if (!sd_manager->exists(path)) {
        Serial.printf("[AudioPlayer] Audio file not found: %s\n", path);
        return false;
    }

    // Read file to get size first
    String wavData = sd_manager->readFile(path);
    if (wavData.isEmpty()) {
        Serial.printf("[AudioPlayer] Failed to read audio file: %s\n", path);
        return false;
    }

    size_t fileSize = wavData.length();

    // Allocate buffer in PSRAM (M5Core2 has 8MB PSRAM)
    uint8_t* psramBuffer = (uint8_t*)ps_malloc(fileSize);
    if (!psramBuffer) {
        Serial.printf("[AudioPlayer] Failed to allocate %d bytes in PSRAM for %s\n", fileSize, path);
        return false;
    }

    // Copy WAV data to PSRAM buffer
    memcpy(psramBuffer, wavData.c_str(), fileSize);

    // Return buffer and size
    *buffer = psramBuffer;
    *len = fileSize;

    Serial.printf("[AudioPlayer] Loaded %s (%d bytes) to PSRAM\n", path, fileSize);
    return true;
}

void AudioPlayer::freeSDBuffers() {
    if (sd_wav_work_start) {
        free(sd_wav_work_start);
        sd_wav_work_start = nullptr;
        sd_wav_work_start_len = 0;
    }

    if (sd_wav_rest_start) {
        free(sd_wav_rest_start);
        sd_wav_rest_start = nullptr;
        sd_wav_rest_start_len = 0;
    }

    if (sd_wav_long_rest_start) {
        free(sd_wav_long_rest_start);
        sd_wav_long_rest_start = nullptr;
        sd_wav_long_rest_start_len = 0;
    }

    if (sd_wav_warning) {
        free(sd_wav_warning);
        sd_wav_warning = nullptr;
        sd_wav_warning_len = 0;
    }

    sd_audio_loaded = false;
    Serial.println("[AudioPlayer] Freed SD audio buffers");
}
