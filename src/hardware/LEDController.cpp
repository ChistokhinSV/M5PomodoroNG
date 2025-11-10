#include "LEDController.h"
#include <Arduino.h>

// FastLED hardware array (GPIO 25, SK6812, GRB order)
#define LED_DATA_PIN 25
CRGB LEDController::fastled_array[LEDController::LED_COUNT];

LEDController::LEDController()
    : current_pattern(Pattern::OFF),
      pattern_color(Color::White()),
      brightness(50) {
    // Initialize LED array to off
    clear();
}

bool LEDController::begin() {
    // MP-23: Enable 5V boost for LED bar power (AXP192 PMIC)
    // M5Stack Core2 LED bar requires external 5V, disabled by default
    M5.Power.setExtOutput(true);  // Replaces deprecated setExtPower()
    Serial.println("[LEDController] 5V boost enabled for LED bar");

    // MP-23: Allow AXP192 time to stabilize 5V output (critical for LED bar)
    delay(100);

    // Verify power output is enabled
    bool power_status = M5.Power.getExtOutput();
    Serial.printf("[LEDController] 5V output verified: %s\n", power_status ? "ENABLED" : "DISABLED");

    // MP-23: Initialize FastLED for SK6812 LEDs on GPIO 25
    // Use WS2812B timing (compatible) but force RGB-only mode (ignore W channel)
    // SK6812 chipset assumes RGBW but we only control RGB channels
    FastLED.addLeds<SK6812, LED_DATA_PIN, GRB>(fastled_array, LED_COUNT);

    // Set normal brightness
    FastLED.setBrightness(map(brightness, 0, 100, 0, 255));

    Serial.println("[LEDController] Hardware initialized (SK6812 LED bar)");
    Serial.printf("[LEDController] GPIO %d, %d LEDs, SK6812/GRB, Brightness %d%%\n",
                  LED_DATA_PIN, LED_COUNT, brightness);

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
    // MP-23: Apply brightness and push to FastLED hardware
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        Color adjusted = applyBrightness(leds[i]);
        fastled_array[i] = CRGB(adjusted.r, adjusted.g, adjusted.b);
    }

    FastLED.show();
}

void LEDController::setBrightness(uint8_t percent) {
    brightness = constrain(percent, 0, 100);
    Serial.printf("[LEDController] User brightness set to %d%%\n", brightness);
}

void LEDController::setPowerMode(PowerMode mode) {
    // MP-23: Apply power mode brightness multiplier (dual brightness system)
    switch (mode) {
        case PowerMode::PERFORMANCE:
            power_mode_multiplier = 100;  // No reduction
            Serial.println("[LEDController] Power mode: PERFORMANCE (100%)");
            break;
        case PowerMode::BALANCED:
            power_mode_multiplier = 60;   // 60% brightness
            Serial.println("[LEDController] Power mode: BALANCED (60%)");
            break;
        case PowerMode::BATTERY_SAVER:
            power_mode_multiplier = 30;   // 30% brightness
            Serial.println("[LEDController] Power mode: BATTERY_SAVER (30%)");
            break;
    }

    Serial.printf("[LEDController] Effective brightness: %d%% × %d%% = %d%%\n",
                 brightness, power_mode_multiplier,
                 (brightness * power_mode_multiplier) / 100);
}

void LEDController::setPattern(Pattern pattern, Color color) {
    // MP-23: Don't override milestone celebration
    if (milestone_active) {
        Serial.println("[LEDController] Pattern change ignored (milestone active)");
        return;
    }

    current_pattern = pattern;
    pattern_color = color;
    animation_step = 0;
    last_update_ms = millis();

    Serial.printf("[LEDController] Pattern: %s, Color: (%d,%d,%d)\n",
                  patternName(pattern), color.r, color.g, color.b);
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

void LEDController::setStatePattern(ILEDController::TimerState state) {
    // MP-23: Don't override milestone celebration
    if (milestone_active) {
        Serial.println("[LEDController] State pattern change ignored (milestone active)");
        return;
    }

    // MP-23: Map timer state to LED pattern + color
    switch (state) {
        case TimerState::IDLE:
            setPattern(Pattern::OFF);
            Serial.println("[LEDController] State: IDLE -> OFF");
            break;

        case TimerState::WORK_ACTIVE:
            // RED pulse for work sessions (user choice: classic Pomodoro)
            setPattern(Pattern::PULSE, Color::Red());
            Serial.println("[LEDController] State: WORK_ACTIVE -> RED PULSE");
            break;

        case TimerState::BREAK_ACTIVE:
            // GREEN pulse for break sessions
            setPattern(Pattern::PULSE, Color::Green());
            Serial.println("[LEDController] State: BREAK_ACTIVE -> GREEN PULSE");
            break;

        case TimerState::PAUSED:
            // RED blink when paused
            setPattern(Pattern::BLINK, Color::Red());
            Serial.println("[LEDController] State: PAUSED -> RED BLINK");
            break;

        case TimerState::WARNING:
            // YELLOW flash for warnings (3× burst @ 200ms)
            setPattern(Pattern::FLASH, Color::Yellow());
            Serial.println("[LEDController] State: WARNING -> YELLOW FLASH");
            break;
    }
}

void LEDController::triggerMilestone(uint32_t duration_ms) {
    // MP-27: Trigger confetti celebration after 4-session cycle completion
    if (!milestone_active) {
        // Save current pattern to restore later
        saved_pattern = current_pattern;
        saved_color = pattern_color;

        // Set pattern BEFORE activating milestone flag (otherwise setPattern rejects it!)
        current_pattern = Pattern::CONFETTI;
        animation_step = 0;
        last_update_ms = millis();

        // Now activate milestone protection
        milestone_active = true;
        milestone_end_ms = millis() + duration_ms;

        Serial.printf("[LEDController] Milestone triggered! %s for %lu ms\n",
                     patternName(Pattern::CONFETTI), duration_ms);
    }
}

void LEDController::update() {
    // MP-23: Check milestone expiration first
    if (milestone_active && millis() >= milestone_end_ms) {
        // Milestone ended - turn off LEDs and release lock
        // State machine will set correct pattern on next update
        milestone_active = false;
        setPattern(Pattern::OFF);
        Serial.println("[LEDController] Milestone ended, LEDs off (awaiting state pattern)");
    }

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
            if (delta >= 50) {  // Update every 50ms for smooth animation
                updateRainbow();
                last_update_ms = now;
            }
            break;

        case Pattern::CONFETTI:
            if (delta >= 30) {  // Update every 30ms for dynamic sparkles
                updateConfetti();
                last_update_ms = now;
            }
            break;

        case Pattern::BLINK:
            if (delta >= 500) {  // Blink every 500ms
                updateBlink();
                last_update_ms = now;
            }
            break;

        case Pattern::FLASH:
            if (delta >= 200) {  // Update every 200ms
                updateFlash();
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
    // Increment hue for smooth rainbow rotation
    animation_step = (animation_step + 4) % 256;

    // Use FastLED's built-in rainbow fill
    // deltaHue = 256/10 = 25.6 per LED (full spectrum across 10 LEDs)
    fill_rainbow(fastled_array, LED_COUNT, animation_step, 256 / LED_COUNT);

    // Apply dual brightness system (user × power mode)
    uint16_t effective_brightness = (brightness * power_mode_multiplier) / 100;
    uint8_t brightness_scale = map(effective_brightness, 0, 100, 0, 255);

    for (uint8_t i = 0; i < LED_COUNT; i++) {
        fastled_array[i].nscale8(brightness_scale);
    }

    FastLED.show();

    // Debug logging (every 10 frames)
    static uint8_t frame_count = 0;
    if (++frame_count >= 10) {
        frame_count = 0;
        Serial.printf("[Rainbow] Hue=%d, Brightness=%d%%, LED0: R=%d G=%d B=%d\n",
                     animation_step, effective_brightness,
                     fastled_array[0].r, fastled_array[0].g, fastled_array[0].b);
    }
}

void LEDController::updateConfetti() {
    // MP-27: Random colored sparkles that fade smoothly on black background
    // Creates a "confetti" celebration effect with vibrant colors

    // Fade all LEDs toward black (creates smooth trails)
    fadeToBlackBy(fastled_array, LED_COUNT, 10);

    // Pick random LED and add random colored sparkle
    uint8_t pos = random16(LED_COUNT);

    // Increment base hue for color variation over time
    animation_step = (animation_step + 1) % 256;

    // Add sparkle with random hue variation (±64 hue units)
    // High saturation (200) and full brightness (255) for vibrant colors
    fastled_array[pos] += CHSV(animation_step + random8(64), 200, 255);

    // Apply dual brightness system (user × power mode)
    uint16_t effective_brightness = (brightness * power_mode_multiplier) / 100;
    uint8_t brightness_scale = map(effective_brightness, 0, 100, 0, 255);

    for (uint8_t i = 0; i < LED_COUNT; i++) {
        fastled_array[i].nscale8(brightness_scale);
    }

    FastLED.show();

    // Debug logging (every 20 frames)
    static uint8_t confetti_frame_count = 0;
    if (++confetti_frame_count >= 20) {
        confetti_frame_count = 0;
        Serial.printf("[Confetti] BaseHue=%d, Brightness=%d%%, Sparkle LED=%d\n",
                     animation_step, effective_brightness, pos);
    }
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

void LEDController::updateFlash() {
    // MP-23: Flash pattern - 3× burst @ 200ms, then 1 sec pause
    // Total cycle: 6 steps (bursts) + 5 steps (pause) = 11 steps @ 200ms each
    animation_step = (animation_step + 1) % 11;

    if (animation_step < 6) {
        // Burst phase: on-off-on-off-on-off (3 bursts)
        if (animation_step % 2 == 0) {
            setAll(pattern_color);
        } else {
            clear();
        }
    } else {
        // Pause phase (steps 6-10): all off
        clear();
    }

    show();
}

LEDController::Color LEDController::applyBrightness(Color color) const {
    // MP-23: Dual brightness system - user setting × power mode multiplier
    // Example: 80% user × 60% balanced = 48% final brightness
    uint16_t effective_brightness = (brightness * power_mode_multiplier) / 100;

    return {
        static_cast<uint8_t>((color.r * effective_brightness) / 100),
        static_cast<uint8_t>((color.g * effective_brightness) / 100),
        static_cast<uint8_t>((color.b * effective_brightness) / 100)
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

const char* LEDController::patternName(Pattern pattern) const {
    switch (pattern) {
        case Pattern::OFF: return "OFF";
        case Pattern::SOLID: return "SOLID";
        case Pattern::PROGRESS: return "PROGRESS";
        case Pattern::PULSE: return "PULSE";
        case Pattern::RAINBOW: return "RAINBOW";
        case Pattern::CONFETTI: return "CONFETTI";
        case Pattern::BLINK: return "BLINK";
        case Pattern::FLASH: return "FLASH";
        default: return "UNKNOWN";
    }
}
