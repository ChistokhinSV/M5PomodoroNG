#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <M5Unified.h>
#include <cstdint>

/**
 * Audio playback via M5Stack Core2 internal speaker (I2S)
 *
 * Features:
 * - WAV file playback from SPIFFS
 * - Volume control (0-100%)
 * - Sound effects (beep, tick, alert)
 * - Non-blocking playback
 *
 * M5Stack Core2 Audio Hardware:
 * - Speaker: NS4168 I2S amplifier
 * - Connector: 8-pin FPC to speaker
 * - Sample Rate: 44.1kHz or 48kHz
 * - Bit Depth: 16-bit
 *
 * Sound Files (data/audio/):
 * - work_start.wav - Work session beginning
 * - work_end.wav - Work session complete
 * - break_start.wav - Break beginning
 * - break_end.wav - Break complete
 * - tick.wav - Timer tick (optional)
 */
class AudioPlayer {
public:
    enum class Sound {
        WORK_START,
        WORK_END,
        BREAK_START,
        BREAK_END,
        TICK,
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

    // Must call every loop iteration
    void update();

private:
    uint8_t current_volume = 70;  // 0-100%
    bool muted = false;
    bool playing = false;

    // WAV file paths
    static constexpr const char* SOUND_PATHS[] = {
        "/audio/work_start.wav",
        "/audio/work_end.wav",
        "/audio/break_start.wav",
        "/audio/break_end.wav",
        "/audio/tick.wav"
    };

    // Internal methods
    bool playWavFile(const char* path);
    void setVolumeInternal(uint8_t volume_255);
};

#endif // AUDIO_PLAYER_H
