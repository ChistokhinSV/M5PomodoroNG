#include "Toggle.h"
#include <M5Unified.h>
#include <stdio.h>
#include <string.h>

Toggle::Toggle()
    : state_(false),
      touch_started_(false) {

    strcpy(label_, "");
}

void Toggle::setState(bool state) {
    if (state_ != state) {
        state_ = state;
        markDirty();
    }
}

void Toggle::setLabel(const char* label) {
    if (label) {
        strncpy(label_, label, sizeof(label_) - 1);
        label_[sizeof(label_) - 1] = '\0';
        markDirty();
    }
}

void Toggle::update(uint32_t deltaMs) {
    // No animations for now
}

void Toggle::draw(Renderer& renderer) {
    if (!visible_) return;

    // Draw label (left side, aligned to top for CSS-style layout)
    if (strlen(label_) > 0) {
        renderer.setTextDatum(TL_DATUM);
        renderer.drawString(bounds_.x, bounds_.y + 2,
                           label_, &fonts::Font2,
                           Renderer::Color(TFT_WHITE));
    }

    // Calculate switch position (right side, aligned to top)
    int16_t switch_w = 60;
    int16_t switch_h = 20;
    int16_t switch_x = bounds_.x + bounds_.w - switch_w;
    int16_t switch_y = bounds_.y;  // Align to top

    // Draw switch background
    Renderer::Color bg_color = state_ ? Renderer::Color(COLOR_ON) : Renderer::Color(COLOR_OFF);
    renderer.drawRect(switch_x, switch_y, switch_w, switch_h, bg_color, true);

    // Draw switch border
    renderer.drawRect(switch_x, switch_y, switch_w, switch_h,
                     Renderer::Color(COLOR_BORDER), false);

    // Draw text inside switch
    const char* text = state_ ? "ON" : "OFF";
    renderer.setTextDatum(MC_DATUM);  // Middle-center
    renderer.drawString(switch_x + switch_w / 2, switch_y + switch_h / 2,
                       text, &fonts::Font2,
                       Renderer::Color(TFT_WHITE));

    clearDirty();
}

void Toggle::onTouch(int16_t x, int16_t y) {
    if (!enabled_) return;

    // Check if touch is on the switch area
    int16_t switch_w = 60;
    int16_t switch_h = 20;
    int16_t switch_x = bounds_.x + bounds_.w - switch_w;
    int16_t switch_y = bounds_.y;  // Align to top

    if (x >= switch_x && x <= switch_x + switch_w &&
        y >= switch_y && y <= switch_y + switch_h) {
        touch_started_ = true;
    }
}

void Toggle::onRelease(int16_t x, int16_t y) {
    if (!enabled_ || !touch_started_) {
        touch_started_ = false;
        return;
    }

    // Check if release is still on the switch area (complete tap)
    int16_t switch_w = 60;
    int16_t switch_h = 20;
    int16_t switch_x = bounds_.x + bounds_.w - switch_w;
    int16_t switch_y = bounds_.y;  // Align to top

    if (x >= switch_x && x <= switch_x + switch_w &&
        y >= switch_y && y <= switch_y + switch_h) {
        // Toggle state
        state_ = !state_;
        markDirty();

        // Fire callback
        if (callback_) {
            callback_(state_);
        }
    }

    touch_started_ = false;
}
