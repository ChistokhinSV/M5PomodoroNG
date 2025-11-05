#ifndef SLIDER_H
#define SLIDER_H

#include "Widget.h"
#include <functional>

/**
 * Slider widget for numeric value selection
 *
 * Features:
 * - Touch dragging to adjust value
 * - Arrow buttons for ±1 increments
 * - Visual track with thumb indicator
 * - Value display (percentage, time, numeric)
 * - Min/max bounds with validation
 * - Callback on value change
 *
 * Layout (320px × 32px recommended):
 * ┌─────────────────────────────────┐
 * │ [−] Label: [====|===] Val [+]   │
 * └─────────────────────────────────┘
 *  35px ~40px   ~150px  ~50px 35px
 *
 * Usage:
 *   Slider slider;
 *   slider.setBounds(20, 50, 280, 32);
 *   slider.setRange(0, 100);
 *   slider.setValue(50);
 *   slider.setLabel("Brightness");
 *   slider.setCallback([](uint16_t val) { ... });
 */
class Slider : public Widget {
public:
    enum class DisplayMode {
        NUMERIC,      // "50"
        PERCENTAGE,   // "50%"
        TIME_MIN,     // "50m"
        TIME_SEC      // "50s"
    };

    using Callback = std::function<void(uint16_t)>;

    Slider();

    // Lifecycle
    void draw(Renderer& renderer) override;
    void update(uint32_t deltaMs) override;
    void onTouch(int16_t x, int16_t y) override;
    void onRelease(int16_t x, int16_t y) override;

    // Configuration
    void setRange(uint16_t min, uint16_t max);
    void setValue(uint16_t value);
    void setLabel(const char* label);
    void setDisplayMode(DisplayMode mode) { display_mode_ = mode; markDirty(); }
    void setCallback(Callback callback) { callback_ = callback; }

    // Query
    uint16_t getValue() const { return value_; }
    uint16_t getMin() const { return min_; }
    uint16_t getMax() const { return max_; }

private:
    uint16_t value_;
    uint16_t min_;
    uint16_t max_;
    char label_[32];
    DisplayMode display_mode_;
    Callback callback_;

    // Touch tracking
    bool dragging_;
    int16_t last_touch_x_;
    bool dec_btn_pressed_;
    bool inc_btn_pressed_;

    // Layout constants
    static constexpr int16_t ARROW_BTN_WIDTH = 35;
    static constexpr int16_t ARROW_BTN_HEIGHT = 32;  // Full widget height (32px)
    static constexpr int16_t ARROW_SPACING = 5;
    static constexpr int16_t VALUE_DISPLAY_WIDTH = 50;

    // Helper methods
    uint16_t xToValue(int16_t x) const;
    int16_t valueToX(uint16_t value) const;
    void formatValue(char* buffer, size_t size) const;
    void getTrackBounds(int16_t& track_x, int16_t& track_w) const;
    void getArrowBounds(int16_t& dec_x, int16_t& inc_x, int16_t& btn_w, int16_t& btn_h) const;
    void handleIncrement();
    void handleDecrement();
};

#endif // SLIDER_H
