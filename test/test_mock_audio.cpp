/**
 * Example Unit Test: MockAudioPlayer (MP-49)
 *
 * Demonstrates usage of MockAudioPlayer for testing audio interactions
 * without actual M5Stack hardware.
 *
 * Test scenarios:
 * - Basic audio playback tracking
 * - Sound history verification
 * - Volume and mute state
 * - Tone generation
 */

#include <gtest/gtest.h>
#include "mocks/MockAudioPlayer.h"

/**
 * Test: MockAudioPlayer tracks play() calls correctly
 */
TEST(MockAudioPlayerTest, TracksPlayCalls) {
    MockAudioPlayer audio;

    // Initially no sounds played
    EXPECT_EQ(0, audio.playCount());
    EXPECT_FALSE(audio.isPlaying());

    // Play work start sound
    audio.play(IAudioPlayer::Sound::WORK_START);

    // Verify tracking
    EXPECT_EQ(1, audio.playCount());
    EXPECT_TRUE(audio.isPlaying());
    EXPECT_EQ(IAudioPlayer::Sound::WORK_START, audio.lastSound());
    EXPECT_TRUE(audio.wasPlayed(IAudioPlayer::Sound::WORK_START));
}

/**
 * Test: MockAudioPlayer tracks sound history
 */
TEST(MockAudioPlayerTest, TracksSoundHistory) {
    MockAudioPlayer audio;

    // Play sequence of sounds
    audio.play(IAudioPlayer::Sound::WORK_START);
    audio.play(IAudioPlayer::Sound::WARNING);
    audio.play(IAudioPlayer::Sound::REST_START);

    // Verify history
    EXPECT_EQ(3, audio.playCount());
    const auto& history = audio.soundHistory();
    ASSERT_EQ(3u, history.size());
    EXPECT_EQ(IAudioPlayer::Sound::WORK_START, history[0]);
    EXPECT_EQ(IAudioPlayer::Sound::WARNING, history[1]);
    EXPECT_EQ(IAudioPlayer::Sound::REST_START, history[2]);
}

/**
 * Test: MockAudioPlayer handles volume control
 */
TEST(MockAudioPlayerTest, HandlesVolume) {
    MockAudioPlayer audio;

    // Default volume
    EXPECT_EQ(70, audio.getVolume());  // Default: 70%

    // Set volume
    audio.setVolume(50);
    EXPECT_EQ(50, audio.getVolume());
    EXPECT_EQ(1, audio.setVolumeCount());

    // Volume clamping (max 100%)
    audio.setVolume(150);
    EXPECT_EQ(100, audio.getVolume());
}

/**
 * Test: MockAudioPlayer handles mute/unmute
 */
TEST(MockAudioPlayerTest, HandlesMute) {
    MockAudioPlayer audio;

    // Initially not muted
    EXPECT_FALSE(audio.isMuted());

    // Mute
    audio.mute();
    EXPECT_TRUE(audio.isMuted());
    EXPECT_EQ(1, audio.muteCount());

    // Unmute
    audio.unmute();
    EXPECT_FALSE(audio.isMuted());
    EXPECT_EQ(1, audio.unmuteCount());
}

/**
 * Test: MockAudioPlayer tracks tone generation
 */
TEST(MockAudioPlayerTest, TracksTones) {
    MockAudioPlayer audio;

    // Play tone
    audio.playTone(1000, 500);

    // Verify tracking
    EXPECT_EQ(1, audio.playToneCount());
    EXPECT_EQ(1000, audio.lastToneFreq());
    EXPECT_EQ(500, audio.lastToneDuration());
    EXPECT_TRUE(audio.isPlaying());
}

/**
 * Test: MockAudioPlayer beep convenience method
 */
TEST(MockAudioPlayerTest, PlayBeep) {
    MockAudioPlayer audio;

    audio.playBeep();

    // Verify beep delegates to playTone
    EXPECT_EQ(1, audio.playBeepCount());
    EXPECT_EQ(1, audio.playToneCount());  // playBeep calls playTone internally
    EXPECT_EQ(1000, audio.lastToneFreq());  // 1kHz
    EXPECT_EQ(200, audio.lastToneDuration());  // 200ms
}

/**
 * Test: MockAudioPlayer reset() clears all state
 */
TEST(MockAudioPlayerTest, ResetClearsState) {
    MockAudioPlayer audio;

    // Make some calls
    audio.play(IAudioPlayer::Sound::WORK_START);
    audio.setVolume(50);
    audio.mute();

    // Reset
    audio.reset();

    // Verify clean state
    EXPECT_EQ(0, audio.playCount());
    EXPECT_EQ(70, audio.getVolume());  // Default volume
    EXPECT_FALSE(audio.isMuted());
    EXPECT_EQ(0u, audio.soundHistory().size());
}

/**
 * Test: MockAudioPlayer configurable begin() result
 */
TEST(MockAudioPlayerTest, ConfigurableBeginResult) {
    MockAudioPlayer audio;

    // Success (default)
    EXPECT_TRUE(audio.begin());
    EXPECT_TRUE(audio.beginCalled());

    // Configure failure
    audio.reset();
    audio.setBeginResult(false);
    EXPECT_FALSE(audio.begin());
}
