#ifndef MOCK_LED_CONTROLLER_H
#define MOCK_LED_CONTROLLER_H

#include "../../src/hardware/ILEDController.h"
#include <vector>
#include <cstring>

/**
 * Mock LED Controller for Unit Testing (MP-49)
 *
 * Provides fake implementation of ILEDController for testing without hardware.
 *
 * Features:
 * - Tracks all method calls for verification
 * - Stores LED state (brightness, pattern, colors)
 * - Counts method calls (setPattern, setAll, etc.)
 * - Simulates LED buffer and brightness
 * - No actual hardware control
 *
 * Usage in Tests:
 *   MockLEDController leds;
 *   leds.setPattern(ILEDController::Pattern::PULSE, ILEDController::Color::Red());
 *
 *   ASSERT_EQ(1, leds.setPatternCount());
 *   ASSERT_EQ(ILEDController::Pattern::PULSE, leds.lastPattern());
 *   ASSERT_TRUE(leds.wasPatternSet(ILEDController::Pattern::PULSE));
 */
class MockLEDController : public ILEDController {
public:
    MockLEDController() {
        // Initialize LED buffer to black
        for (uint8_t i = 0; i < LED_COUNT; i++) {
            leds_[i] = Color::Black();
        }
    }

    // ========================================
    // ILEDController Interface Implementation
    // ========================================

    bool begin() override {
        begin_called_ = true;
        return begin_result_;
    }

    void setAll(Color color) override {
        for (uint8_t i = 0; i < LED_COUNT; i++) {
            leds_[i] = color;
        }
        last_color_ = color;
        set_all_count_++;
    }

    void setPixel(uint8_t index, Color color) override {
        if (index < LED_COUNT) {
            leds_[index] = color;
            last_pixel_index_ = index;
            last_pixel_color_ = color;
            set_pixel_count_++;
        }
    }

    void clear() override {
        for (uint8_t i = 0; i < LED_COUNT; i++) {
            leds_[i] = Color::Black();
        }
        clear_count_++;
    }

    void show() override {
        show_count_++;
    }

    void setBrightness(uint8_t percent) override {
        brightness_ = (percent > 100) ? 100 : percent;
        set_brightness_count_++;
    }

    uint8_t getBrightness() const override {
        return brightness_;
    }

    void setPattern(Pattern pattern, Color color = Color::White()) override {
        current_pattern_ = pattern;
        last_pattern_color_ = color;
        set_pattern_count_++;
        pattern_history_.push_back(pattern);
    }

    void setProgress(uint8_t percent, Color color = Color::Green()) override {
        progress_percent_ = (percent > 100) ? 100 : percent;
        progress_color_ = color;
        set_progress_count_++;
    }

    void update() override {
        update_count_++;
    }

    // ========================================
    // Test Inspection Methods
    // ========================================

    /**
     * Check if begin() was called
     */
    bool beginCalled() const { return begin_called_; }

    /**
     * Get current pattern
     */
    Pattern currentPattern() const { return current_pattern_; }

    /**
     * Get last pattern color
     */
    Color lastPatternColor() const { return last_pattern_color_; }

    /**
     * Get last color set via setAll()
     */
    Color lastColor() const { return last_color_; }

    /**
     * Get color of specific LED
     */
    Color getPixel(uint8_t index) const {
        if (index < LED_COUNT) {
            return leds_[index];
        }
        return Color::Black();
    }

    /**
     * Get last pixel index set via setPixel()
     */
    uint8_t lastPixelIndex() const { return last_pixel_index_; }

    /**
     * Get last pixel color set via setPixel()
     */
    Color lastPixelColor() const { return last_pixel_color_; }

    /**
     * Get progress percentage
     */
    uint8_t progressPercent() const { return progress_percent_; }

    /**
     * Get progress color
     */
    Color progressColor() const { return progress_color_; }

    /**
     * Get number of setAll() calls
     */
    int setAllCount() const { return set_all_count_; }

    /**
     * Get number of setPixel() calls
     */
    int setPixelCount() const { return set_pixel_count_; }

    /**
     * Get number of clear() calls
     */
    int clearCount() const { return clear_count_; }

    /**
     * Get number of show() calls
     */
    int showCount() const { return show_count_; }

    /**
     * Get number of setBrightness() calls
     */
    int setBrightnessCount() const { return set_brightness_count_; }

    /**
     * Get number of setPattern() calls
     */
    int setPatternCount() const { return set_pattern_count_; }

    /**
     * Get number of setProgress() calls
     */
    int setProgressCount() const { return set_progress_count_; }

    /**
     * Get number of update() calls
     */
    int updateCount() const { return update_count_; }

    /**
     * Check if specific pattern was ever set
     */
    bool wasPatternSet(Pattern pattern) const {
        for (const auto& p : pattern_history_) {
            if (p == pattern) return true;
        }
        return false;
    }

    /**
     * Get complete pattern history
     */
    const std::vector<Pattern>& patternHistory() const {
        return pattern_history_;
    }

    /**
     * Check if two colors are equal
     */
    static bool colorsEqual(const Color& a, const Color& b) {
        return a.r == b.r && a.g == b.g && a.b == b.b;
    }

    /**
     * Reset all state for next test
     */
    void reset() {
        begin_called_ = false;
        brightness_ = 50;
        current_pattern_ = Pattern::OFF;
        last_pattern_color_ = Color::White();
        last_color_ = Color::Black();
        last_pixel_index_ = 0;
        last_pixel_color_ = Color::Black();
        progress_percent_ = 0;
        progress_color_ = Color::Green();

        set_all_count_ = 0;
        set_pixel_count_ = 0;
        clear_count_ = 0;
        show_count_ = 0;
        set_brightness_count_ = 0;
        set_pattern_count_ = 0;
        set_progress_count_ = 0;
        update_count_ = 0;

        pattern_history_.clear();

        for (uint8_t i = 0; i < LED_COUNT; i++) {
            leds_[i] = Color::Black();
        }
    }

    /**
     * Configure begin() to return specific result
     */
    void setBeginResult(bool result) {
        begin_result_ = result;
    }

private:
    // State
    bool begin_called_ = false;
    bool begin_result_ = true;
    uint8_t brightness_ = 50;
    Pattern current_pattern_ = Pattern::OFF;
    Color leds_[LED_COUNT];

    // Last values
    Color last_pattern_color_ = Color::White();
    Color last_color_ = Color::Black();
    uint8_t last_pixel_index_ = 0;
    Color last_pixel_color_ = Color::Black();
    uint8_t progress_percent_ = 0;
    Color progress_color_ = Color::Green();

    // Call counters
    int set_all_count_ = 0;
    int set_pixel_count_ = 0;
    int clear_count_ = 0;
    int show_count_ = 0;
    int set_brightness_count_ = 0;
    int set_pattern_count_ = 0;
    int set_progress_count_ = 0;
    int update_count_ = 0;

    // History
    std::vector<Pattern> pattern_history_;
};

#endif // MOCK_LED_CONTROLLER_H
