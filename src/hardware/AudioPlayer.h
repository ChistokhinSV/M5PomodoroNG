#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

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
 * Features:
 * - WAV file playback from PROGMEM (embedded in firmware)
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
 * Embedded Sounds (PROGMEM):
 * - WORK_START - Work session beginning (ascending tones)
 * - REST_START - Short break beginning (descending tones)
 * - LONG_REST_START - Long break beginning (long descending sequence)
 * - WARNING - 30-second warning (double beep)
 */
class AudioPlayer {
public:
    enum class Sound {
        WORK_START,
        REST_START,
        LONG_REST_START,
        WARNING,
        BEEP_SHORT,    // Generated tone (200ms)
        BEEP_LONG      // Generated tone (500ms)
    };

    AudioPlayer();

    // Initialization
    bool begin();

    // Playback control
    void play(Sound sound);
    void stop();
    bool isPlaying() const;

    // Volume control (0-100%)
    void setVolume(uint8_t percent);
    uint8_t getVolume() const { return current_volume; }

    // Mute control
    void mute();
    void unmute();
    bool isMuted() const { return muted; }

    // Generated tones (for testing without WAV files)
    void playTone(uint16_t frequency_hz, uint16_t duration_ms);
    void playBeep();  // Quick 1kHz beep

    // Warning sound (30s before timer expiration)
    void playWarning();

    // Must call every loop iteration
    void update();

private:
    uint8_t current_volume = 70;  // 0-100%
    bool muted = false;
    bool playing = false;

    // Internal methods
    bool playWavFile(const uint8_t* wav_data, size_t len);
    void setVolumeInternal(uint8_t volume_255);
};

#endif // AUDIO_PLAYER_H
