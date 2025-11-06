#ifndef I_AUDIO_PLAYER_H
#define I_AUDIO_PLAYER_H

#include <cstdint>

/**
 * Audio Player Interface - Hardware Abstraction for Testing (MP-49)
 *
 * Provides abstract interface for audio playback, allowing:
 * - Unit testing with mock implementations
 * - Hardware-independent screen and state machine tests
 * - Dependency injection for better testability
 *
 * Usage:
 *   // Production code uses concrete AudioPlayer
 *   IAudioPlayer* audio = new AudioPlayer();
 *   audio->play(IAudioPlayer::Sound::WORK_START);
 *
 *   // Test code uses MockAudioPlayer
 *   MockAudioPlayer mockAudio;
 *   screen.setAudio(&mockAudio);
 *   // ... verify mockAudio.lastSound == WORK_START
 */
class IAudioPlayer {
public:
    /**
     * Predefined sound effects
     */
    enum class Sound {
        WORK_START,         // Work session beginning (ascending tones)
        REST_START,         // Short break beginning (descending tones)
        LONG_REST_START,    // Long break beginning (long descending sequence)
        WARNING,            // 30-second warning (double beep)
        BEEP_SHORT,         // Generated tone (200ms, 1kHz)
        BEEP_LONG           // Generated tone (500ms, 1kHz)
    };

    virtual ~IAudioPlayer() = default;

    // ====================
    // Initialization
    // ====================

    /**
     * Initialize audio hardware
     * @return true if successful, false on failure
     */
    virtual bool begin() = 0;

    // ====================
    // Playback Control
    // ====================

    /**
     * Play predefined sound effect
     * @param sound Sound to play
     */
    virtual void play(Sound sound) = 0;

    /**
     * Stop current playback
     */
    virtual void stop() = 0;

    /**
     * Check if audio is currently playing
     * @return true if playing, false if idle
     */
    virtual bool isPlaying() const = 0;

    // ====================
    // Volume Control
    // ====================

    /**
     * Set volume level
     * @param percent Volume (0-100%)
     */
    virtual void setVolume(uint8_t percent) = 0;

    /**
     * Get current volume level
     * @return Volume (0-100%)
     */
    virtual uint8_t getVolume() const = 0;

    // ====================
    // Mute Control
    // ====================

    /**
     * Mute audio output
     */
    virtual void mute() = 0;

    /**
     * Unmute audio output
     */
    virtual void unmute() = 0;

    /**
     * Check if audio is muted
     * @return true if muted, false otherwise
     */
    virtual bool isMuted() const = 0;

    // ====================
    // Generated Tones
    // ====================

    /**
     * Play tone at specified frequency and duration
     * @param frequency_hz Tone frequency (Hz)
     * @param duration_ms Tone duration (milliseconds)
     */
    virtual void playTone(uint16_t frequency_hz, uint16_t duration_ms) = 0;

    /**
     * Play quick beep (1kHz, 200ms)
     */
    virtual void playBeep() = 0;

    /**
     * Play warning sound (30s before timer expiration)
     */
    virtual void playWarning() = 0;

    // ====================
    // Update Loop
    // ====================

    /**
     * Update audio state (must call every loop iteration)
     * Tracks playback state for non-blocking operation
     */
    virtual void update() = 0;
};

#endif // I_AUDIO_PLAYER_H
