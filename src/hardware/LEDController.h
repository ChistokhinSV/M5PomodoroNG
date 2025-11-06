#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include "ILEDController.h"
#include <M5Unified.h>
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

    // Brightness control (0-100%)
    void setBrightness(uint8_t percent) override;
    uint8_t getBrightness() const override { return brightness; }

    // Pattern control
    void setPattern(Pattern pattern, Color color = Color::White()) override;
    void setProgress(uint8_t percent, Color color = Color::Green()) override;

    // Animation update (call every loop)
    void update() override;

private:
    Color leds[LED_COUNT];
    Pattern current_pattern = Pattern::OFF;
    Color pattern_color = Color::White();
    uint8_t brightness = 50;  // 0-100%

    // Animation state
    uint32_t last_update_ms = 0;
    uint8_t animation_step = 0;

    // Internal methods
    void updatePattern();
    void updatePulse();
    void updateRainbow();
    void updateBlink();
    Color applyBrightness(Color color) const;
    Color wheelColor(uint8_t pos) const;  // Rainbow wheel helper
};

#endif // LED_CONTROLLER_H
