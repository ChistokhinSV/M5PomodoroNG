#ifndef HMI_CONTROLLER_H
#define HMI_CONTROLLER_H

#include <M5Unified.h>
#include <cstdint>
#include "IHapticController.h"

/**
 * HMI (Human-Machine Interface) controller for encoder + buttons
 *
 * M5Stack Core2 does NOT have a hardware encoder by default.
 * However, it supports:
 * - 3 touch zones (bottom of screen acts as buttons A/B/C)
 * - Optional HMI unit via Grove connector (rotary encoder + 3 buttons)
 *
 * This controller abstracts:
 * - Touch zones as button presses
 * - Optional encoder input (if HMI unit connected)
 * - Button state tracking (pressed, released, held)
 *
 * Touch zones (Core2 built-in):
 * - Zone A: Left third (0-106px)
 * - Zone B: Middle third (107-213px)
 * - Zone C: Right third (214-320px)
 *
 * Optional HMI Unit (I2C 0x5F):
 * - Rotary encoder (rotation + button press)
 * - 3 additional buttons (Red, Yellow, Green)
 */
class HMIController {
public:
    enum class Button {
        NONE,
        BUTTON_A,      // Touch zone A / HMI button 1
        BUTTON_B,      // Touch zone B / HMI button 2
        BUTTON_C,      // Touch zone C / HMI button 3
        ENCODER_BTN    // HMI encoder button (press)
    };

    enum class ButtonState {
        RELEASED,
        PRESSED,
        HELD         // Held for > 500ms
    };

    struct EncoderState {
        int16_t position;      // Rotation count (increments CW, decrements CCW)
        int16_t delta;         // Change since last read
        bool button_pressed;
    };

    HMIController();

    // Initialization
    bool begin();

    // Must call every loop iteration
    void update();

    // Touch zone buttons (always available)
    bool isPressed(Button button) const;
    bool wasPressed(Button button);      // Check & clear pressed flag
    bool wasReleased(Button button);     // Check & clear released flag
    bool isHeld(Button button) const;

    // Encoder (if HMI unit connected)
    bool hasEncoder() const { return encoder_available; }
    EncoderState getEncoder() const { return encoder; }
    int16_t getEncoderDelta();           // Get & reset delta

    // Haptic controller (MP-27)
    void setHapticController(IHapticController* controller) { haptic_controller = controller; }

    // Haptic feedback (via vibration motor) - DEPRECATED, use setHapticController() instead
    void hapticFeedback(uint8_t strength = 50, uint16_t duration_ms = 50);

private:
    // Button state tracking
    struct ButtonInfo {
        ButtonState state = ButtonState::RELEASED;
        bool pressed_flag = false;
        bool released_flag = false;
        uint32_t press_start_ms = 0;
    };

    ButtonInfo buttons[4];  // A, B, C, Encoder
    bool encoder_available = false;
    EncoderState encoder = {0, 0, false};
    IHapticController* haptic_controller = nullptr;  // MP-27: Haptic feedback

    // I2C HMI unit address (if connected)
    static constexpr uint8_t HMI_I2C_ADDR = 0x5F;
    static constexpr uint32_t HOLD_THRESHOLD_MS = 500;

    // Internal methods
    void updateTouchZones();
    void updateEncoder();
    void updateButtonState(Button button, bool is_pressed);
    Button getTouchZone(int16_t x, int16_t y) const;
};

#endif // HMI_CONTROLLER_H
