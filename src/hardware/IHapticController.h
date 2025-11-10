#ifndef IHAPTICCONTROLLER_H
#define IHAPTICCONTROLLER_H

#include <stdint.h>

/**
 * Interface for haptic feedback control (vibration motor)
 *
 * M5Stack Core2 vibration motor is controlled via AXP192 PM

IC LDO3 output.
 * Concrete implementation uses M5.Power.Axp192.setLDO3() for motor control.
 *
 * Patterns:
 * - BUTTON_PRESS: 50ms single buzz (tactile feedback)
 * - TIMER_COMPLETE: 3× 200ms buzzes with 100ms gaps (rhythmic notification)
 * - GESTURE: 100ms single buzz (gesture detected confirmation)
 * - ERROR: 2× 50ms buzzes with 50ms gap (error indication)
 *
 * Usage:
 *   IHapticController* haptic = new HapticController(config);
 *   haptic->begin();
 *
 *   // In main loop
 *   haptic->update();
 *
 *   // Trigger vibration
 *   haptic->trigger(IHapticController::Pattern::BUTTON_PRESS);
 *
 *   // Enable/disable via settings
 *   haptic->setEnabled(config.ui.haptic_enabled);
 */
class IHapticController {
public:
    /**
     * Vibration patterns for different feedback scenarios
     */
    enum class Pattern {
        BUTTON_PRESS,   // 50ms single buzz - tactile button feedback
        TIMER_COMPLETE, // 3× 200ms buzzes (100ms gaps) - timer finished
        GESTURE,        // 100ms single buzz - gyro gesture detected
        ERROR,         // 2× 50ms buzzes (50ms gap) - error notification
        CYCLE_COMPLETE  // 5× 80ms buzzes (40ms gaps) - celebratory full cycle finish
    };

    virtual ~IHapticController() = default;

    /**
     * Initialize haptic hardware
     * @return true if initialization successful
     */
    virtual bool begin() = 0;

    /**
     * Trigger a vibration pattern
     * @param pattern Vibration pattern to execute
     *
     * Non-blocking: Pattern execution handled by update() method.
     * If haptic disabled via setEnabled(false), this is a no-op.
     */
    virtual void trigger(Pattern pattern) = 0;

    /**
     * Update vibration state machine (call in main loop)
     *
     * Handles non-blocking vibration timing for rhythm patterns.
     * Must be called frequently (every loop iteration) for accurate timing.
     */
    virtual void update() = 0;

    /**
     * Enable or disable haptic feedback
     * @param enabled true to enable vibrations, false to disable
     *
     * When disabled, trigger() calls are ignored.
     * Typically controlled by user settings (config.ui.haptic_enabled).
     */
    virtual void setEnabled(bool enabled) = 0;

    /**
     * Check if haptic feedback is currently enabled
     * @return true if enabled, false if disabled
     */
    virtual bool isEnabled() const = 0;
};

#endif // IHAPTICCONTROLLER_H
