#include "Button.h"
#include <M5Unified.h>
#include <string.h>

Button::Button()
    : callback_(nullptr),
      pressed_(false),
      normal_color_(Renderer::Color(0x4A90E2)),    // Blue
      pressed_color_(Renderer::Color(0x2E5C8A)),   // Darker blue
      text_color_(Renderer::Color(TFT_WHITE)) {
    label_[0] = '\0';
}

Button::Button(const char* label)
    : Button() {
    setLabel(label);
}

void Button::setLabel(const char* label) {
    if (label) {
        strncpy(label_, label, sizeof(label_) - 1);
        label_[sizeof(label_) - 1] = '\0';
        markDirty();
    }
}

void Button::setCallback(void (*callback)()) {
    callback_ = callback;
}

void Button::setColors(Renderer::Color normal, Renderer::Color pressed) {
    normal_color_ = normal;
    pressed_color_ = pressed;
    markDirty();
}

void Button::draw(Renderer& renderer) {
    if (!visible_) return;

    // Choose color based on state
    Renderer::Color bg_color = enabled_ ?
        (pressed_ ? pressed_color_ : normal_color_) :
        Renderer::Color(0x666666);  // Gray when disabled

    // Draw button background (filled rounded rect approximation)
    renderer.drawRect(bounds_.x, bounds_.y, bounds_.w, bounds_.h, bg_color, true);

    // Draw border
    if (!pressed_) {
        renderer.drawRect(bounds_.x, bounds_.y, bounds_.w, bounds_.h,
                         Renderer::Color(TFT_WHITE), false);
    }

    // Draw label (centered)
    if (label_[0] != '\0') {
        renderer.setTextDatum(MC_DATUM);  // Middle-center
        int16_t center_x = bounds_.x + bounds_.w / 2;
        int16_t center_y = bounds_.y + bounds_.h / 2;
        renderer.drawString(center_x, center_y, label_, &fonts::Font2, text_color_);
    }

    clearDirty();
}

void Button::onTouch(int16_t x, int16_t y) {
    if (!enabled_) return;

    pressed_ = true;
    markDirty();
}

void Button::onRelease(int16_t x, int16_t y) {
    if (!enabled_) return;

    if (pressed_) {
        pressed_ = false;
        markDirty();

        // Fire callback if touch released within button bounds
        if (hitTest(x, y) && callback_) {
            callback_();
        }
    }
}
