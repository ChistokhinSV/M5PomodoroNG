#ifndef MOCK_AUDIO_PLAYER_H
#define MOCK_AUDIO_PLAYER_H

#include "../../src/hardware/IAudioPlayer.h"
#include <vector>

/**
 * Mock Audio Player for Unit Testing (MP-49)
 *
 * Provides fake implementation of IAudioPlayer for testing without hardware.
 *
 * Features:
 * - Tracks all method calls for verification
 * - Stores last sound played
 * - Counts play/stop calls
 * - Simulates volume and mute state
 * - No actual audio playback
 *
 * Usage in Tests:
 *   MockAudioPlayer audio;
 *   audio.play(IAudioPlayer::Sound::WORK_START);
 *
 *   ASSERT_EQ(1, audio.playCount());
 *   ASSERT_EQ(IAudioPlayer::Sound::WORK_START, audio.lastSound());
 *   ASSERT_TRUE(audio.wasPlayed(IAudioPlayer::Sound::WORK_START));
 */
class MockAudioPlayer : public IAudioPlayer {
public:
    MockAudioPlayer() = default;

    // ========================================
    // IAudioPlayer Interface Implementation
    // ========================================

    bool begin() override {
        begin_called_ = true;
        return begin_result_;
    }

    void play(Sound sound) override {
        last_sound_ = sound;
        playing_ = true;
        play_count_++;
        sound_history_.push_back(sound);
    }

    void stop() override {
        playing_ = false;
        stop_count_++;
    }

    bool isPlaying() const override {
        return playing_;
    }

    void setVolume(uint8_t percent) override {
        volume_ = (percent > 100) ? 100 : percent;
        set_volume_count_++;
    }

    uint8_t getVolume() const override {
        return volume_;
    }

    void mute() override {
        muted_ = true;
        mute_count_++;
    }

    void unmute() override {
        muted_ = false;
        unmute_count_++;
    }

    bool isMuted() const override {
        return muted_;
    }

    void playTone(uint16_t frequency_hz, uint16_t duration_ms) override {
        last_tone_freq_ = frequency_hz;
        last_tone_duration_ = duration_ms;
        playing_ = true;
        play_tone_count_++;
    }

    void playBeep() override {
        play_beep_count_++;
        playTone(1000, 200);  // 1kHz, 200ms
    }

    void playWarning() override {
        play_warning_count_++;
        play(Sound::WARNING);
    }

    void update() override {
        update_count_++;
        // Mock: automatically stop after simulated playback
        if (playing_ && update_count_ > 10) {
            playing_ = false;
        }
    }

    // ========================================
    // Test Inspection Methods
    // ========================================

    /**
     * Check if begin() was called
     */
    bool beginCalled() const { return begin_called_; }

    /**
     * Get last sound played
     */
    Sound lastSound() const { return last_sound_; }

    /**
     * Get number of play() calls
     */
    int playCount() const { return play_count_; }

    /**
     * Get number of stop() calls
     */
    int stopCount() const { return stop_count_; }

    /**
     * Get number of update() calls
     */
    int updateCount() const { return update_count_; }

    /**
     * Get number of setVolume() calls
     */
    int setVolumeCount() const { return set_volume_count_; }

    /**
     * Get number of mute() calls
     */
    int muteCount() const { return mute_count_; }

    /**
     * Get number of unmute() calls
     */
    int unmuteCount() const { return unmute_count_; }

    /**
     * Get number of playTone() calls
     */
    int playToneCount() const { return play_tone_count_; }

    /**
     * Get number of playBeep() calls
     */
    int playBeepCount() const { return play_beep_count_; }

    /**
     * Get number of playWarning() calls
     */
    int playWarningCount() const { return play_warning_count_; }

    /**
     * Get last tone frequency (Hz)
     */
    uint16_t lastToneFreq() const { return last_tone_freq_; }

    /**
     * Get last tone duration (ms)
     */
    uint16_t lastToneDuration() const { return last_tone_duration_; }

    /**
     * Check if specific sound was ever played
     */
    bool wasPlayed(Sound sound) const {
        for (const auto& s : sound_history_) {
            if (s == sound) return true;
        }
        return false;
    }

    /**
     * Get complete sound history
     */
    const std::vector<Sound>& soundHistory() const {
        return sound_history_;
    }

    /**
     * Reset all state for next test
     */
    void reset() {
        begin_called_ = false;
        playing_ = false;
        volume_ = 70;
        muted_ = false;
        last_sound_ = Sound::BEEP_SHORT;
        play_count_ = 0;
        stop_count_ = 0;
        update_count_ = 0;
        set_volume_count_ = 0;
        mute_count_ = 0;
        unmute_count_ = 0;
        play_tone_count_ = 0;
        play_beep_count_ = 0;
        play_warning_count_ = 0;
        last_tone_freq_ = 0;
        last_tone_duration_ = 0;
        sound_history_.clear();
    }

    /**
     * Configure begin() to return specific result
     */
    void setBeginResult(bool result) {
        begin_result_ = result;
    }

private:
    // State
    bool begin_called_ = false;
    bool begin_result_ = true;
    bool playing_ = false;
    uint8_t volume_ = 70;
    bool muted_ = false;

    // Last values
    Sound last_sound_ = Sound::BEEP_SHORT;
    uint16_t last_tone_freq_ = 0;
    uint16_t last_tone_duration_ = 0;

    // Call counters
    int play_count_ = 0;
    int stop_count_ = 0;
    int update_count_ = 0;
    int set_volume_count_ = 0;
    int mute_count_ = 0;
    int unmute_count_ = 0;
    int play_tone_count_ = 0;
    int play_beep_count_ = 0;
    int play_warning_count_ = 0;

    // History
    std::vector<Sound> sound_history_;
};

#endif // MOCK_AUDIO_PLAYER_H
