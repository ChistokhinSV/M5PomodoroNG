#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include "IAudioPlayer.h"
#include "SDManager.h"
#include <M5Unified.h>
#include <cstdint>

// Forward declarations for embedded audio data (from audio_data.cpp)
extern const uint8_t wav_work_start[] PROGMEM;
extern const uint32_t wav_work_start_len;
extern const uint8_t wav_rest_start[] PROGMEM;
extern const uint32_t wav_rest_start_len;
extern const uint8_t wav_long_rest_start[] PROGMEM;
extern const uint32_t wav_long_rest_start_len;
extern const uint8_t wav_warning[] PROGMEM;
extern const uint32_t wav_warning_len;

/**
 * Audio playback via M5Stack Core2 internal speaker (I2S)
 *
 * Implements IAudioPlayer interface for hardware abstraction (MP-49)
 *
 * Features:
 * - WAV file playback from SD card (primary) or PROGMEM (fallback)
 * - Volume control (0-100%)
 * - Sound effects (beep tones)
 * - Non-blocking playback
 *
 * M5Stack Core2 Audio Hardware:
 * - Speaker: NS4168 I2S amplifier
 * - Sample Rate: 16kHz
 * - Bit Depth: 16-bit signed PCM
 * - Power: AXP192 controlled (GPIO enable)
 *
 * Audio Sources (MP-71):
 * - PRIMARY: SD card /audio/*.wav files (loaded to PSRAM)
 * - FALLBACK: PROGMEM embedded WAV data (always available)
 *
 * Sounds:
 * - WORK_START - Work session beginning (ascending tones)
 * - REST_START - Short break beginning (descending tones)
 * - LONG_REST_START - Long break beginning (long descending sequence)
 * - WARNING - 30-second warning (double beep)
 */
class AudioPlayer : public IAudioPlayer {
public:
    enum class AudioSource {
        FLASH,      // Embedded in firmware flash (fallback)
        SD_CARD,    // Loaded from SD card (preferred)
        AUTO        // Try SD first, fallback to FLASH
    };

    AudioPlayer();
    ~AudioPlayer();

    // Initialization
    bool begin() override;  // Uses AUTO mode (SD fallback to PROGMEM)
    bool begin(SDManager* sd_manager, AudioSource source = AudioSource::AUTO);

    // Playback control
    void play(Sound sound) override;
    void stop() override;
    bool isPlaying() const override;

    // Volume control (0-100%)
    void setVolume(uint8_t percent) override;
    uint8_t getVolume() const override { return current_volume; }

    // Mute control
    void mute() override;
    void unmute() override;
    bool isMuted() const override { return muted; }

    // Generated tones (for testing without WAV files)
    void playTone(uint16_t frequency_hz, uint16_t duration_ms) override;
    void playBeep() override;  // Quick 1kHz beep

    // Warning sound (30s before timer expiration)
    void playWarning() override;

    // Must call every loop iteration
    void update() override;

    // Audio source management (MP-71)
    AudioSource getAudioSource() const { return current_source; }
    bool isSDCardAudioAvailable() const { return sd_audio_loaded; }

private:
    uint8_t current_volume = 70;  // 0-100%
    bool muted = false;
    bool playing = false;

    // SD card support (MP-71)
    SDManager* sd_manager = nullptr;
    AudioSource current_source = AudioSource::FLASH;
    bool sd_audio_loaded = false;

    // SD audio buffers (allocated in PSRAM)
    uint8_t* sd_wav_work_start = nullptr;
    size_t sd_wav_work_start_len = 0;
    uint8_t* sd_wav_rest_start = nullptr;
    size_t sd_wav_rest_start_len = 0;
    uint8_t* sd_wav_long_rest_start = nullptr;
    size_t sd_wav_long_rest_start_len = 0;
    uint8_t* sd_wav_warning = nullptr;
    size_t sd_wav_warning_len = 0;

    // Internal methods
    bool loadAudioFromSD();
    bool loadWavFromSD(const char* path, uint8_t** buffer, size_t* len);
    void freeSDBuffers();
    bool playWavFile(const uint8_t* wav_data, size_t len);
    void setVolumeInternal(uint8_t volume_255);
};

#endif // AUDIO_PLAYER_H
