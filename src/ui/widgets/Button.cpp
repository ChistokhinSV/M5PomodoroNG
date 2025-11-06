#include "Button.h"
#include <M5Unified.h>
#include <string.h>

Button::Button()
    : pressed_(false),
      normal_color_(Renderer::Color(0x4A90E2)),    // Blue
      pressed_color_(Renderer::Color(0x2E5C8A)),   // Darker blue
      text_color_(Renderer::Color(TFT_WHITE)) {
    label_[0] = '\0';
    label_line2_[0] = '\0';
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

void Button::setLabel(const char* line1, const char* line2) {
    if (line1) {
        strncpy(label_, line1, sizeof(label_) - 1);
        label_[sizeof(label_) - 1] = '\0';
    }
    if (line2) {
        strncpy(label_line2_, line2, sizeof(label_line2_) - 1);
        label_line2_[sizeof(label_line2_) - 1] = '\0';
    }
    markDirty();
}

void Button::setCallback(std::function<void()> callback) {
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

    // Draw label (centered, 1 or 2 lines)
    if (label_[0] != '\0') {
        renderer.setTextDatum(MC_DATUM);  // Middle-center
        int16_t center_x = bounds_.x + bounds_.w / 2;
        int16_t center_y = bounds_.y + bounds_.h / 2;

        if (label_line2_[0] != '\0') {
            // Two-line mode: draw line 1 above center, line 2 below
            renderer.drawString(center_x, center_y - 8, label_, &fonts::Font2, text_color_);
            renderer.drawString(center_x, center_y + 8, label_line2_, &fonts::Font2, text_color_);
        } else {
            // Single-line mode: draw at center
            renderer.drawString(center_x, center_y, label_, &fonts::Font2, text_color_);
        }
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
