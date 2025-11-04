#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <M5Unified.h>
#include <cstdint>

/**
 * RGB LED controller for SK6812 LED bar (M5Stack Core2)
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
class LEDController {
public:
    enum class Pattern {
        OFF,            // All LEDs off
        SOLID,          // All LEDs same color
        PROGRESS,       // Fill LEDs proportionally (0-100%)
        PULSE,          // Breathing effect
        RAINBOW,        // Rainbow color cycle
        BLINK           // Blink pattern
    };

    struct Color {
        uint8_t r;
        uint8_t g;
        uint8_t b;

        // Predefined colors
        static Color Black()   { return {0, 0, 0}; }
        static Color Red()     { return {255, 0, 0}; }
        static Color Green()   { return {0, 255, 0}; }
        static Color Blue()    { return {0, 0, 255}; }
        static Color Yellow()  { return {255, 255, 0}; }
        static Color Cyan()    { return {0, 255, 255}; }
        static Color Magenta() { return {255, 0, 255}; }
        static Color White()   { return {255, 255, 255}; }
        static Color Orange()  { return {255, 165, 0}; }
        static Color Purple()  { return {128, 0, 128}; }
    };

    LEDController();

    // Initialization
    bool begin();

    // Basic control
    void setAll(Color color);
    void setPixel(uint8_t index, Color color);
    void clear();
    void show();  // Update LED strip

    // Brightness control (0-100%)
    void setBrightness(uint8_t percent);
    uint8_t getBrightness() const { return brightness; }

    // Pattern control
    void setPattern(Pattern pattern, Color color = Color::White());
    void setProgress(uint8_t percent, Color color = Color::Green());

    // Animation update (call every loop)
    void update();

    // LED count
    static constexpr uint8_t LED_COUNT = 10;

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
