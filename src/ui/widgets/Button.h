#ifndef BUTTON_H
#define BUTTON_H

#include "Widget.h"

/**
 * Touch button widget with text label
 *
 * Features:
 * - Text label (centered)
 * - Visual feedback on press (color change)
 * - Rounded corners (4px radius)
 * - Optional callback on press
 * - Minimum touch target: 44×44px recommended
 *
 * States: NORMAL, PRESSED, DISABLED
 */
class Button : public Widget {
public:
    Button();
    explicit Button(const char* label);

    // Configuration
    void setLabel(const char* label);
    void setCallback(void (*callback)());
    void setColors(Renderer::Color normal, Renderer::Color pressed);

    // Widget interface
    void draw(Renderer& renderer) override;
    void onTouch(int16_t x, int16_t y) override;
    void onRelease(int16_t x, int16_t y) override;

private:
    char label_[16];
    void (*callback_)();
    bool pressed_;

    Renderer::Color normal_color_;
    Renderer::Color pressed_color_;
    Renderer::Color text_color_;
};

#endif // BUTTON_H
