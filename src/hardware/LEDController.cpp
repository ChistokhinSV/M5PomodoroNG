#include "LEDController.h"
#include <Arduino.h>

LEDController::LEDController()
    : current_pattern(Pattern::OFF),
      pattern_color(Color::White()),
      brightness(50) {
    // Initialize LED array to off
    clear();
}

bool LEDController::begin() {
    // Note: M5Stack Core2 LED control depends on specific hardware implementation
    // This is a placeholder - actual implementation would use:
    // - FastLED library for SK6812 control
    // - GPIO 25 as data pin
    // - Color order: GRB for SK6812

    Serial.println("[LEDController] Initialized (SK6812 LED bar)");
    Serial.println("[LEDController] WARNING: Actual LED hardware control not yet implemented");
    Serial.println("[LEDController] GPIO 25, 10 LEDs, SK6812 protocol");

    // Clear all LEDs
    clear();
    show();

    return true;
}

void LEDController::setAll(Color color) {
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        leds[i] = color;
    }
}

void LEDController::setPixel(uint8_t index, Color color) {
    if (index < LED_COUNT) {
        leds[index] = color;
    }
}

void LEDController::clear() {
    setAll(Color::Black());
}

void LEDController::show() {
    // Apply brightness and send to hardware
    // Placeholder: Actual implementation would use FastLED.show()

    // For now, just log (to avoid spam, only log on change)
    static uint32_t last_log = 0;
    if (millis() - last_log > 5000) {
        Serial.print("[LEDController] LEDs: ");
        for (uint8_t i = 0; i < LED_COUNT; i++) {
            Color adjusted = applyBrightness(leds[i]);
            if (adjusted.r > 0 || adjusted.g > 0 || adjusted.b > 0) {
                Serial.printf("#%d:(%d,%d,%d) ", i, adjusted.r, adjusted.g, adjusted.b);
            }
        }
        Serial.println();
        last_log = millis();
    }
}

void LEDController::setBrightness(uint8_t percent) {
    brightness = constrain(percent, 0, 100);
    Serial.printf("[LEDController] Brightness set to %d%%\n", brightness);
}

void LEDController::setPattern(Pattern pattern, Color color) {
    current_pattern = pattern;
    pattern_color = color;
    animation_step = 0;
    last_update_ms = millis();

    Serial.printf("[LEDController] Pattern: %d, Color: (%d,%d,%d)\n",
                  static_cast<int>(pattern), color.r, color.g, color.b);
}

void LEDController::setProgress(uint8_t percent, Color color) {
    percent = constrain(percent, 0, 100);

    // Map 0-100% to 0-10 LEDs
    uint8_t lit_count = (percent * LED_COUNT) / 100;

    clear();
    for (uint8_t i = 0; i < lit_count; i++) {
        setPixel(i, color);
    }

    show();
}

void LEDController::update() {
    updatePattern();
}

// Private methods

void LEDController::updatePattern() {
    uint32_t now = millis();
    uint32_t delta = now - last_update_ms;

    switch (current_pattern) {
        case Pattern::OFF:
            clear();
            show();
            break;

        case Pattern::SOLID:
            setAll(pattern_color);
            show();
            break;

        case Pattern::PULSE:
            if (delta >= 50) {  // Update every 50ms
                updatePulse();
                last_update_ms = now;
            }
            break;

        case Pattern::RAINBOW:
            if (delta >= 100) {  // Update every 100ms
                updateRainbow();
                last_update_ms = now;
            }
            break;

        case Pattern::BLINK:
            if (delta >= 500) {  // Blink every 500ms
                updateBlink();
                last_update_ms = now;
            }
            break;

        case Pattern::PROGRESS:
            // Progress is set manually via setProgress()
            break;
    }
}

void LEDController::updatePulse() {
    // Breathing effect: fade in/out
    const uint8_t steps = 50;
    animation_step = (animation_step + 1) % (steps * 2);

    uint8_t pulse_brightness;
    if (animation_step < steps) {
        // Fade in
        pulse_brightness = (animation_step * 100) / steps;
    } else {
        // Fade out
        pulse_brightness = ((steps * 2 - animation_step) * 100) / steps;
    }

    // Temporarily adjust brightness for pulse
    uint8_t saved_brightness = brightness;
    brightness = pulse_brightness;

    setAll(pattern_color);
    show();

    brightness = saved_brightness;
}

void LEDController::updateRainbow() {
    animation_step = (animation_step + 1) % 256;

    for (uint8_t i = 0; i < LED_COUNT; i++) {
        // Offset each LED in the rainbow
        uint8_t hue = (animation_step + (i * 256 / LED_COUNT)) % 256;
        setPixel(i, wheelColor(hue));
    }

    show();
}

void LEDController::updateBlink() {
    animation_step = (animation_step + 1) % 2;

    if (animation_step == 0) {
        setAll(pattern_color);
    } else {
        clear();
    }

    show();
}

LEDController::Color LEDController::applyBrightness(Color color) const {
    return {
        static_cast<uint8_t>((color.r * brightness) / 100),
        static_cast<uint8_t>((color.g * brightness) / 100),
        static_cast<uint8_t>((color.b * brightness) / 100)
    };
}

LEDController::Color LEDController::wheelColor(uint8_t pos) const {
    // Rainbow color wheel (0-255 maps to full spectrum)
    pos = 255 - pos;

    if (pos < 85) {
        return {static_cast<uint8_t>(255 - pos * 3), 0, static_cast<uint8_t>(pos * 3)};
    } else if (pos < 170) {
        pos -= 85;
        return {0, static_cast<uint8_t>(pos * 3), static_cast<uint8_t>(255 - pos * 3)};
    } else {
        pos -= 170;
        return {static_cast<uint8_t>(pos * 3), static_cast<uint8_t>(255 - pos * 3), 0};
    }
}
