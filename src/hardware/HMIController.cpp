#include "HMIController.h"
#include <Arduino.h>

HMIController::HMIController()
    : encoder_available(false) {
}

bool HMIController::begin() {
    // Check if HMI unit is connected via I2C
    // Placeholder: Actual implementation would probe I2C bus
    encoder_available = false;  // Assume not connected for now

    Serial.println("[HMIController] Initialized");
    Serial.println("[HMIController] Touch zones: A/B/C enabled");

    if (encoder_available) {
        Serial.println("[HMIController] HMI encoder detected");
    } else {
        Serial.println("[HMIController] No HMI encoder (touch only)");
    }

    return true;
}

void HMIController::update() {
    updateTouchZones();

    if (encoder_available) {
        updateEncoder();
    }
}

bool HMIController::isPressed(Button button) const {
    if (button == Button::NONE) return false;
    uint8_t idx = static_cast<uint8_t>(button) - 1;
    if (idx >= 4) return false;

    return buttons[idx].state == ButtonState::PRESSED ||
           buttons[idx].state == ButtonState::HELD;
}

bool HMIController::wasPressed(Button button) {
    if (button == Button::NONE) return false;
    uint8_t idx = static_cast<uint8_t>(button) - 1;
    if (idx >= 4) return false;

    bool result = buttons[idx].pressed_flag;
    buttons[idx].pressed_flag = false;  // Clear flag
    return result;
}

bool HMIController::wasReleased(Button button) {
    if (button == Button::NONE) return false;
    uint8_t idx = static_cast<uint8_t>(button) - 1;
    if (idx >= 4) return false;

    bool result = buttons[idx].released_flag;
    buttons[idx].released_flag = false;  // Clear flag
    return result;
}

bool HMIController::isHeld(Button button) const {
    if (button == Button::NONE) return false;
    uint8_t idx = static_cast<uint8_t>(button) - 1;
    if (idx >= 4) return false;

    return buttons[idx].state == ButtonState::HELD;
}

int16_t HMIController::getEncoderDelta() {
    int16_t delta = encoder.delta;
    encoder.delta = 0;  // Reset delta after reading
    return delta;
}

void HMIController::hapticFeedback(uint8_t strength, uint16_t duration_ms) {
    // MP-27: Use injected haptic controller instead of placeholder
    if (haptic_controller) {
        haptic_controller->trigger(IHapticController::Pattern::BUTTON_PRESS);
    } else {
        Serial.println("[HMIController] WARNING: No haptic controller set");
    }
}

// Private methods

void HMIController::updateTouchZones() {
    // Get touch state from M5.Touch
    auto touch = M5.Touch.getDetail();

    // Check if touch is active
    if (touch.isPressed() || touch.isHolding()) {
        Button zone = getTouchZone(touch.x, touch.y);

        if (zone != Button::NONE) {
            updateButtonState(zone, true);
        }
    } else if (touch.wasReleased()) {
        // Released - update all buttons
        for (uint8_t i = 0; i < 3; i++) {  // Only touch zones A/B/C
            if (buttons[i].state != ButtonState::RELEASED) {
                updateButtonState(static_cast<Button>(i + 1), false);
            }
        }
    }
}

void HMIController::updateEncoder() {
    // Placeholder: Read encoder position from I2C HMI unit
    // Actual implementation would:
    // 1. Read I2C register from 0x5F
    // 2. Parse encoder count (16-bit)
    // 3. Calculate delta from last position
    // 4. Read button state

    // For now, encoder is not implemented
}

void HMIController::updateButtonState(Button button, bool is_pressed) {
    uint8_t idx = static_cast<uint8_t>(button) - 1;
    if (idx >= 4) return;

    ButtonInfo& btn = buttons[idx];
    uint32_t now = millis();

    if (is_pressed) {
        if (btn.state == ButtonState::RELEASED) {
            // Newly pressed
            btn.state = ButtonState::PRESSED;
            btn.pressed_flag = true;
            btn.press_start_ms = now;

            Serial.printf("[HMIController] Button %d pressed\n", idx);

            // Haptic feedback on press
            hapticFeedback(30, 30);
        } else if (btn.state == ButtonState::PRESSED) {
            // Check if held long enough
            if (now - btn.press_start_ms >= HOLD_THRESHOLD_MS) {
                btn.state = ButtonState::HELD;
                Serial.printf("[HMIController] Button %d held\n", idx);
            }
        }
    } else {
        if (btn.state != ButtonState::RELEASED) {
            // Released
            btn.state = ButtonState::RELEASED;
            btn.released_flag = true;
            btn.press_start_ms = 0;

            Serial.printf("[HMIController] Button %d released\n", idx);
        }
    }
}

HMIController::Button HMIController::getTouchZone(int16_t x, int16_t y) const {
    // M5Stack Core2 display: 320×240
    // Touch zones at bottom (y > 200):
    // A: 0-106
    // B: 107-213
    // C: 214-320

    if (y < 200) {
        return Button::NONE;  // Not in button zone
    }

    if (x < 107) {
        return Button::BUTTON_A;
    } else if (x < 214) {
        return Button::BUTTON_B;
    } else {
        return Button::BUTTON_C;
    }
}
