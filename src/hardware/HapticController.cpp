#include "HapticController.h"
#include <M5Unified.h>
#include <Arduino.h>

HapticController::HapticController(Config& config)
    : config_(config),
      enabled_(true),  // Will be updated from config in begin()
      state_(State::IDLE),
      state_start_ms_(0),
      pulse_index_(0),
      total_pulses_(0),
      gap_duration_ms_(0) {
    // Initialize pulse_durations_ array to zero
    for (uint8_t i = 0; i < 8; i++) {
        pulse_durations_[i] = 0;
    }
}

bool HapticController::begin() {
    Serial.println("[HapticController] Initializing haptic feedback...");

    // Load enabled state from config
    enabled_ = config_.getUI().haptic_enabled;

    // Verify M5Stack Core2 board (vibration motor only on Core2)
    if (M5.getBoard() != m5::board_t::board_M5StackCore2) {
        Serial.println("[HapticController] WARNING: Not M5Stack Core2, vibration motor may not be available");
        // Continue anyway (won't crash, just won't vibrate)
    }

    // Ensure motor is OFF initially
    stopMotor();

    Serial.printf("[HapticController] Initialized (enabled: %s)\n", enabled_ ? "true" : "false");
    return true;
}

void HapticController::trigger(Pattern pattern) {
    // Ignore if disabled
    if (!enabled_) {
        Serial.println("[HapticController] Trigger ignored (haptic disabled)");
        return;
    }

    // Ignore if already vibrating (don't interrupt current pattern)
    if (state_ != State::IDLE) {
        Serial.println("[HapticController] Trigger ignored (pattern already active)");
        return;
    }

    // Start pattern based on type
    switch (pattern) {
        case Pattern::BUTTON_PRESS:
            Serial.println("[HapticController] Trigger: BUTTON_PRESS (50ms)");
            startPattern(50);  // 50ms single buzz
            break;

        case Pattern::TIMER_COMPLETE:
            Serial.println("[HapticController] Trigger: TIMER_COMPLETE (3× 200ms)");
            startRhythmPattern(3, 200, 100);  // 3× 200ms with 100ms gaps
            break;

        case Pattern::GESTURE:
            Serial.println("[HapticController] Trigger: GESTURE (100ms)");
            startPattern(100);  // 100ms single buzz
            break;

        case Pattern::ERROR:
            Serial.println("[HapticController] Trigger: ERROR (2× 50ms)");
            startRhythmPattern(2, 50, 50);  // 2× 50ms with 50ms gap
            break;

        case Pattern::CYCLE_COMPLETE:
            Serial.println("[HapticController] Trigger: CYCLE_COMPLETE (5× 80ms) - Happy celebration!");
            startRhythmPattern(5, 80, 40);  // 5× 80ms with 40ms gaps - celebratory pattern
            break;

        default:
            Serial.println("[HapticController] WARNING: Unknown pattern");
            break;
    }
}

void HapticController::update() {
    if (state_ == State::IDLE) {
        return;  // Nothing to do
    }

    uint32_t now = millis();
    uint32_t elapsed = now - state_start_ms_;

    switch (state_) {
        case State::VIBRATING: {
            // Check if current pulse duration has elapsed
            uint16_t current_duration = pulse_durations_[pulse_index_];
            if (elapsed >= current_duration) {
                stopMotor();

                // Check if there are more pulses in the pattern
                if (pulse_index_ < total_pulses_ - 1) {
                    // Move to next pulse (after gap)
                    pulse_index_++;
                    state_ = State::PAUSED;
                    state_start_ms_ = now;
                } else {
                    // Pattern complete
                    state_ = State::IDLE;
                    Serial.println("[HapticController] Pattern complete");
                }
            }
            break;
        }

        case State::PAUSED: {
            // Check if gap duration has elapsed
            if (elapsed >= gap_duration_ms_) {
                // Start next pulse
                startMotor();
                state_ = State::VIBRATING;
                state_start_ms_ = now;
            }
            break;
        }

        case State::IDLE:
            // Should not reach here (early return at top)
            break;
    }
}

void HapticController::setEnabled(bool enabled) {
    enabled_ = enabled;

    // If disabling while vibrating, stop immediately
    if (!enabled_ && state_ != State::IDLE) {
        stopMotor();
        state_ = State::IDLE;
        Serial.println("[HapticController] Disabled mid-pattern, motor stopped");
    }

    Serial.printf("[HapticController] Haptic feedback %s\n", enabled_ ? "enabled" : "disabled");
}

bool HapticController::isEnabled() const {
    return enabled_;
}

// Private methods

void HapticController::startMotor() {
    // Turn ON vibration motor via AXP192 LDO3
    // Core2 vibration motor requires 3.3V on LDO3
    M5.Power.Axp192.setLDO3(3300);  // 3300mV = 3.3V
    Serial.println("[HapticController] Motor ON");
}

void HapticController::stopMotor() {
    // Turn OFF vibration motor
    M5.Power.Axp192.setLDO3(0);  // 0V = OFF
    // Note: No serial print here to avoid spam in update() loop
}

void HapticController::startPattern(uint16_t single_duration_ms) {
    // Single pulse pattern (no rhythm)
    pulse_index_ = 0;
    total_pulses_ = 1;
    pulse_durations_[0] = single_duration_ms;
    gap_duration_ms_ = 0;  // No gaps for single pulse

    // Start vibrating immediately
    startMotor();
    state_ = State::VIBRATING;
    state_start_ms_ = millis();
}

void HapticController::startRhythmPattern(uint8_t count, uint16_t pulse_ms, uint16_t gap_ms) {
    // Rhythm pattern (multiple pulses with gaps)
    pulse_index_ = 0;
    total_pulses_ = count;
    gap_duration_ms_ = gap_ms;

    // All pulses have same duration
    for (uint8_t i = 0; i < count && i < 8; i++) {
        pulse_durations_[i] = pulse_ms;
    }

    // Start first pulse immediately
    startMotor();
    state_ = State::VIBRATING;
    state_start_ms_ = millis();
}
