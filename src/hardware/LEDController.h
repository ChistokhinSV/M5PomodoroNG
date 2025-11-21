#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include "ILEDController.h"
#include <M5Unified.h>
// enable RGBW support
#define FASTLED_EXPERIMENTAL_ESP32_RGBW_ENABLED 1
#include <FastLED.h>
#include <cstdint>

/**
 * RGB LED controller for SK6812 LED bar (M5Stack Core2)
 *
 * Implements ILEDController interface for hardware abstraction (MP-49)
 *
 * Features:
 * - 10 RGB LEDs (SK6812 addressable)
 * - Color control (RGB888)
 * - Brightness control
 * - Animation patterns (solid, pulse, progress, rainbow)
 *
 * M5Stack Core2 LED Hardware:
 * - 10× SK6812 RGB LEDs
 * - GPIO 25 (data pin)
 * - 5V power
 *
 * Use cases:
 * - Timer progress indicator (fill LEDs as timer counts down)
 * - Status indication (work=red, break=green, paused=yellow)
 * - Notifications (pulse on session complete)
 */
class LEDController : public ILEDController {
public:
    LEDController();

    // Initialization
    bool begin() override;

    // Basic control
    void setAll(Color color) override;
    void setPixel(uint8_t index, Color color) override;
    void clear() override;
    void show() override;  // Update LED strip
    void powerDown() override;  // MP-30: Disable 5V boost before deep sleep

    // Brightness control (0-100%)
    void setBrightness(uint8_t percent) override;
    uint8_t getBrightness() const override { return brightness; }

    // Power mode brightness control (MP-23)
    enum class PowerMode {
        PERFORMANCE,     // 100% brightness (no reduction)
        BALANCED,        // 60% brightness
        BATTERY_SAVER    // 30% brightness
    };
    void setPowerMode(PowerMode mode);

    // Pattern control
    void setPattern(Pattern pattern, Color color = Color::White()) override;
    void setProgress(uint8_t percent, Color color = Color::Green()) override;
    Pattern getPattern() const override { return current_pattern; }  // Get current pattern

    // State-based pattern control (MP-23) - now in interface
    void setStatePattern(ILEDController::TimerState state) override;

    // Milestone celebration (MP-23)
    void triggerMilestone(uint32_t duration_ms = 10000);  // Default 10 sec rainbow

    // Animation update (call every loop)
    void update() override;

private:
    Color leds[LED_COUNT];              // Software LED buffer
    static CRGB fastled_array[LED_COUNT];  // FastLED hardware array (MP-23)
    Pattern current_pattern = Pattern::OFF;
    Color pattern_color = Color::White();
    uint8_t brightness = 50;  // 0-100% (user setting)
    uint8_t power_mode_multiplier = 100;  // MP-23: Power mode brightness (100/60/30%)

    // Animation state
    uint32_t last_update_ms = 0;
    uint8_t animation_step = 0;

    // Flash pattern state (MP-23)
    uint8_t flash_count = 0;     // Current burst (0-2)
    bool flash_on = false;        // LED state during burst

    // Milestone celebration state (MP-23)
    bool milestone_active = false;         // Is milestone rainbow active?
    uint32_t milestone_end_ms = 0;         // When milestone ends
    Pattern saved_pattern = Pattern::OFF;  // Pattern to restore after milestone
    Color saved_color = Color::White();    // Color to restore after milestone

    // Internal methods
    void updatePattern();
    void updatePulse();
    void updateRainbow();
    void updateConfetti();        // MP-27: Random sparkles with fade trails
    void updateBlink();
    void updateFlash();           // MP-23: 3× burst @ 200ms
    Color applyBrightness(Color color) const;
    Color wheelColor(uint8_t pos) const;  // Rainbow wheel helper
    const char* patternName(Pattern pattern) const;  // Pattern enum to string
};

#endif // LED_CONTROLLER_H
