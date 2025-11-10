#ifndef I_LED_CONTROLLER_H
#define I_LED_CONTROLLER_H

#include <cstdint>

/**
 * LED Controller Interface - Hardware Abstraction for Testing (MP-49)
 *
 * Provides abstract interface for RGB LED control, allowing:
 * - Unit testing with mock implementations
 * - Hardware-independent screen tests
 * - Dependency injection for better testability
 *
 * Usage:
 *   // Production code uses concrete LEDController
 *   ILEDController* leds = new LEDController();
 *   leds->setPattern(ILEDController::Pattern::PULSE, ILEDController::Color::Red());
 *
 *   // Test code uses MockLEDController
 *   MockLEDController mockLeds;
 *   screen.setLEDs(&mockLeds);
 *   // ... verify mockLeds.lastPattern == Pattern::PULSE
 */
class ILEDController {
public:
    /**
     * LED animation patterns
     */
    enum class Pattern {
        OFF,            // All LEDs off
        SOLID,          // All LEDs same color
        PROGRESS,       // Fill LEDs proportionally (0-100%)
        PULSE,          // Breathing effect
        RAINBOW,        // Rainbow color cycle
        CONFETTI,       // Random colored sparkles with fade trails (MP-27)
        BLINK,          // Blink pattern
        FLASH           // Flash burst (3× @ 200ms for warnings)
    };

    /**
     * RGB color representation
     */
    struct Color {
        uint8_t r;
        uint8_t g;
        uint8_t b;

        // Predefined colors (static factory methods)
        static Color Black()   { return {0, 0, 0}; }
        static Color Red()     { return {255, 0, 0}; }
        static Color Green()   { return {0, 255, 0}; }
        static Color Blue()    { return {0, 0, 255}; }
        static Color Yellow()  { return {255, 255, 0}; }
        static Color Cyan()    { return {0, 255, 255}; }
        static Color Magenta() { return {255, 0, 255}; }
        static Color White()   { return {255, 255, 255}; }
        static Color Orange()  { return {255, 80, 0}; }   // Less green for true orange
        static Color Amber()   { return {255, 100, 0}; }  // MP-23: Paused state (orange-red)
        static Color Purple()  { return {128, 0, 128}; }
    };

    /**
     * LED count (10 LEDs on M5Stack Core2)
     */
    static constexpr uint8_t LED_COUNT = 10;

    /**
     * Timer state for state-based LED patterns
     */
    enum class TimerState {
        IDLE,
        WORK_ACTIVE,
        BREAK_ACTIVE,
        PAUSED,
        WARNING
    };

    virtual ~ILEDController() = default;

    // ====================
    // Initialization
    // ====================

    /**
     * Initialize LED hardware
     * @return true if successful, false on failure
     */
    virtual bool begin() = 0;

    // ====================
    // Basic Control
    // ====================

    /**
     * Set all LEDs to same color
     * @param color RGB color value
     */
    virtual void setAll(Color color) = 0;

    /**
     * Set specific LED color
     * @param index LED index (0-9)
     * @param color RGB color value
     */
    virtual void setPixel(uint8_t index, Color color) = 0;

    /**
     * Turn off all LEDs
     */
    virtual void clear() = 0;

    /**
     * Update LED strip with current buffer
     * Must be called after setAll/setPixel to push changes
     */
    virtual void show() = 0;

    // ====================
    // Brightness Control
    // ====================

    /**
     * Set brightness level
     * @param percent Brightness (0-100%)
     */
    virtual void setBrightness(uint8_t percent) = 0;

    /**
     * Get current brightness level
     * @return Brightness (0-100%)
     */
    virtual uint8_t getBrightness() const = 0;

    // ====================
    // Pattern Control
    // ====================

    /**
     * Set animation pattern
     * @param pattern Animation pattern (OFF, SOLID, PROGRESS, etc.)
     * @param color Color for pattern (default: white)
     */
    virtual void setPattern(Pattern pattern, Color color = Color::White()) = 0;

    /**
     * Set progress bar pattern
     * @param percent Progress percentage (0-100%)
     * @param color Color for progress bar (default: green)
     */
    virtual void setProgress(uint8_t percent, Color color = Color::Green()) = 0;

    /**
     * Get current pattern
     * @return Current animation pattern
     */
    virtual Pattern getPattern() const = 0;

    /**
     * Set LED pattern based on timer state
     * @param state Timer state (IDLE, WORK_ACTIVE, BREAK_ACTIVE, PAUSED, WARNING)
     */
    virtual void setStatePattern(TimerState state) = 0;

    /**
     * Trigger milestone celebration (rainbow animation)
     * @param duration_ms Duration of celebration in milliseconds (default: 10000ms = 10s)
     *
     * Shows rainbow animation for specified duration, then auto-restores previous pattern.
     * Used for celebrating 4-session cycle completion.
     */
    virtual void triggerMilestone(uint32_t duration_ms = 10000) = 0;

    // ====================
    // Update Loop
    // ====================

    /**
     * Update animation state (must call every loop iteration)
     * Advances animation frames for PULSE, RAINBOW, BLINK patterns
     */
    virtual void update() = 0;
};

#endif // I_LED_CONTROLLER_H
