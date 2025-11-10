#ifndef HAPTICCONTROLLER_H
#define HAPTICCONTROLLER_H

#include "IHapticController.h"
#include "../core/Config.h"

/**
 * Concrete implementation of haptic feedback control for M5Stack Core2
 *
 * Controls vibration motor via AXP192 PMIC LDO3 output.
 * Uses non-blocking state machine for rhythm patterns (e.g., 3× 200ms buzzes).
 *
 * Hardware Details:
 * - Motor: Vibration motor (no PWM intensity control, on/off only)
 * - Power: AXP192 LDO3 @ 3.3V
 * - Control: M5.Power.Axp192.setLDO3(3300) = ON, setLDO3(0) = OFF
 *
 * Implementation:
 * - Non-blocking: update() method handles timing (no delay() calls)
 * - Config-aware: Respects config.ui.haptic_enabled setting
 * - State machine: Tracks IDLE → VIBRATING → PAUSED → VIBRATING → ... → IDLE
 */
class HapticController : public IHapticController {
public:
    /**
     * Constructor
     * @param config Reference to Config object for haptic_enabled setting
     */
    explicit HapticController(Config& config);

    // Implement IHapticController interface
    bool begin() override;
    void trigger(Pattern pattern) override;
    void update() override;
    void setEnabled(bool enabled) override;
    bool isEnabled() const override;

private:
    Config& config_;
    bool enabled_;

    // State machine for non-blocking vibration control
    enum class State {
        IDLE,       // No vibration active
        VIBRATING,  // Motor ON, waiting for duration to elapse
        PAUSED      // Motor OFF, waiting for gap before next pulse (rhythm patterns)
    };

    State state_;
    uint32_t state_start_ms_;  // When current state started (for timing)

    // Current pattern being executed
    uint8_t pulse_index_;       // Current pulse in rhythm (0-based)
    uint8_t total_pulses_;      // Total pulses in pattern
    uint16_t pulse_durations_[8];  // Duration of each pulse (ms), max 8 pulses
    uint16_t gap_duration_ms_;  // Gap between pulses (ms)

    // Hardware control
    void startMotor();
    void stopMotor();

    // Pattern helpers
    void startPattern(uint16_t single_duration_ms);
    void startRhythmPattern(uint8_t count, uint16_t pulse_ms, uint16_t gap_ms);
};

#endif // HAPTICCONTROLLER_H
