#ifndef TOGGLE_H
#define TOGGLE_H

#include "Widget.h"
#include <functional>

/**
 * Toggle switch widget for boolean settings
 *
 * Features:
 * - On/off visual indicator
 * - Touch to flip state
 * - Color-coded (green=on, gray=off)
 * - Label display
 * - Callback on state change
 *
 * Layout:
 * ┌─────────────────────────────────┐
 * │ Label Text         [ON ]        │
 * └─────────────────────────────────┘
 *   20px                  60×20px
 *
 * Usage:
 *   Toggle toggle;
 *   toggle.setBounds(20, 50, 280, 25);
 *   toggle.setLabel("Sound Enabled");
 *   toggle.setState(true);
 *   toggle.setCallback([](bool state) { ... });
 */
class Toggle : public Widget {
public:
    using Callback = std::function<void(bool)>;

    Toggle();

    // Lifecycle
    void draw(Renderer& renderer) override;
    void update(uint32_t deltaMs) override;
    void onTouch(int16_t x, int16_t y) override;
    void onRelease(int16_t x, int16_t y) override;

    // Configuration
    void setState(bool state);
    void setLabel(const char* label);
    void setCallback(Callback callback) { callback_ = callback; }

    // Query
    bool getState() const { return state_; }

private:
    bool state_;
    char label_[32];
    Callback callback_;
    bool touch_started_;  // Track if touch started on this widget

    // Colors (RGB565 format: 5 bits red, 6 bits green, 5 bits blue)
    static constexpr uint16_t COLOR_ON = TFT_GREEN;
    static constexpr uint16_t COLOR_OFF = 0x632C;  // Gray (RGB: 102, 102, 102)
    static constexpr uint16_t COLOR_BORDER = 0x8C51;  // Light gray (RGB: 136, 136, 136)
};

#endif // TOGGLE_H
