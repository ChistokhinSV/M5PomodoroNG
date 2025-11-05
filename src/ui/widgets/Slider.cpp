#include "Slider.h"
#include <M5Unified.h>
#include <stdio.h>
#include <string.h>

Slider::Slider()
    : value_(0),
      min_(0),
      max_(100),
      display_mode_(DisplayMode::NUMERIC),
      dragging_(false),
      last_touch_x_(0),
      dec_btn_pressed_(false),
      inc_btn_pressed_(false) {

    strcpy(label_, "");
}

void Slider::setRange(uint16_t min, uint16_t max) {
    if (min >= max) return;  // Invalid range

    min_ = min;
    max_ = max;

    // Clamp current value to new range
    if (value_ < min_) value_ = min_;
    if (value_ > max_) value_ = max_;

    markDirty();
}

void Slider::setValue(uint16_t value) {
    // Clamp to range
    if (value < min_) value = min_;
    if (value > max_) value = max_;

    if (value_ != value) {
        value_ = value;
        markDirty();
    }
}

void Slider::setLabel(const char* label) {
    if (label) {
        strncpy(label_, label, sizeof(label_) - 1);
        label_[sizeof(label_) - 1] = '\0';
        markDirty();
    }
}

void Slider::update(uint32_t deltaMs) {
    // No animations for now
}

void Slider::draw(Renderer& renderer) {
    if (!visible_) return;

    // Get arrow button bounds
    int16_t dec_x, inc_x, btn_w, btn_h;
    getArrowBounds(dec_x, inc_x, btn_w, btn_h);

    // Draw decrement button (left arrow) - centered vertically
    int16_t btn_y = bounds_.y + (bounds_.h - btn_h) / 2;
    renderer.drawRect(dec_x, btn_y, btn_w, btn_h,
                     Renderer::Color(dec_btn_pressed_ ? 0x4A90E2 : 0x444444), true);
    renderer.drawRect(dec_x, btn_y, btn_w, btn_h,
                     Renderer::Color(0x888888), false);
    renderer.setTextDatum(MC_DATUM);
    renderer.drawString(dec_x + btn_w / 2, btn_y + btn_h / 2,
                       "-", &fonts::Font4, Renderer::Color(TFT_WHITE));

    // Draw increment button (right arrow) - centered vertically
    renderer.drawRect(inc_x, btn_y, btn_w, btn_h,
                     Renderer::Color(inc_btn_pressed_ ? 0x4A90E2 : 0x444444), true);
    renderer.drawRect(inc_x, btn_y, btn_w, btn_h,
                     Renderer::Color(0x888888), false);
    renderer.setTextDatum(MC_DATUM);
    renderer.drawString(inc_x + btn_w / 2, btn_y + btn_h / 2,
                       "+", &fonts::Font4, Renderer::Color(TFT_WHITE));

    // Draw label (after left button, aligned to top for CSS-style layout)
    int16_t label_x = dec_x + btn_w + ARROW_SPACING;
    if (strlen(label_) > 0) {
        renderer.setTextDatum(TL_DATUM);
        renderer.drawString(label_x, bounds_.y + 2,
                           label_, &fonts::Font2,
                           Renderer::Color(TFT_WHITE));
    }

    // Get track bounds (aligned to top of button area)
    int16_t track_x, track_w;
    getTrackBounds(track_x, track_w);
    int16_t track_y = bounds_.y + 4;  // Small offset from top
    int16_t track_h = 8;

    // Draw track background
    renderer.drawRect(track_x, track_y, track_w, track_h,
                     Renderer::Color(0x333333), true);

    // Draw track border
    renderer.drawRect(track_x, track_y, track_w, track_h,
                     Renderer::Color(0x666666), false);

    // Draw filled portion (progress)
    if (max_ > min_) {
        int16_t fill_w = (track_w * (value_ - min_)) / (max_ - min_);
        if (fill_w > 0) {
            renderer.drawRect(track_x + 1, track_y + 1, fill_w - 2, track_h - 2,
                             Renderer::Color(0x4A90E2), true);  // Blue
        }
    }

    // Draw thumb (larger, more visible)
    int16_t thumb_x = valueToX(value_);
    renderer.drawRect(thumb_x - 4, track_y - 2, 8, track_h + 4,
                     Renderer::Color(TFT_WHITE), true);

    // Draw value display (before right button, aligned to top)
    char value_str[16];
    formatValue(value_str, sizeof(value_str));

    int16_t value_x = inc_x - ARROW_SPACING;
    renderer.setTextDatum(TR_DATUM);  // Top-right
    renderer.drawString(value_x, bounds_.y + 2,
                       value_str, &fonts::Font2,
                       Renderer::Color(TFT_WHITE));

    clearDirty();
}

void Slider::onTouch(int16_t x, int16_t y) {
    if (!enabled_) return;

    // Get arrow button bounds
    int16_t dec_x, inc_x, btn_w, btn_h;
    getArrowBounds(dec_x, inc_x, btn_w, btn_h);
    int16_t btn_y = bounds_.y + (bounds_.h - btn_h) / 2;

    // Check if touch is on decrement button
    if (x >= dec_x && x < dec_x + btn_w && y >= btn_y && y < btn_y + btn_h) {
        dec_btn_pressed_ = true;
        markDirty();
        return;
    }

    // Check if touch is on increment button
    if (x >= inc_x && x < inc_x + btn_w && y >= btn_y && y < btn_y + btn_h) {
        inc_btn_pressed_ = true;
        markDirty();
        return;
    }

    // Otherwise, start dragging slider
    int16_t track_x, track_w;
    getTrackBounds(track_x, track_w);

    if (x >= track_x && x < track_x + track_w) {
        dragging_ = true;
        last_touch_x_ = x;

        // Convert touch X to value
        uint16_t new_value = xToValue(x);
        if (new_value != value_) {
            setValue(new_value);
            if (callback_) {
                callback_(value_);
            }
        }
    }
}

void Slider::onRelease(int16_t x, int16_t y) {
    // Get arrow button bounds
    int16_t dec_x, inc_x, btn_w, btn_h;
    getArrowBounds(dec_x, inc_x, btn_w, btn_h);
    int16_t btn_y = bounds_.y + (bounds_.h - btn_h) / 2;

    // Handle decrement button release
    if (dec_btn_pressed_) {
        // Check if release is still on button (complete tap)
        if (x >= dec_x && x < dec_x + btn_w && y >= btn_y && y < btn_y + btn_h) {
            handleDecrement();
        }
        dec_btn_pressed_ = false;
        markDirty();
        return;
    }

    // Handle increment button release
    if (inc_btn_pressed_) {
        // Check if release is still on button (complete tap)
        if (x >= inc_x && x < inc_x + btn_w && y >= btn_y && y < btn_y + btn_h) {
            handleIncrement();
        }
        inc_btn_pressed_ = false;
        markDirty();
        return;
    }

    // Handle slider drag release
    if (dragging_) {
        // Final value update
        uint16_t new_value = xToValue(x);
        if (new_value != value_) {
            setValue(new_value);
            if (callback_) {
                callback_(value_);
            }
        }
        dragging_ = false;
    }
}

uint16_t Slider::xToValue(int16_t x) const {
    // Get track bounds
    int16_t track_x, track_w;
    getTrackBounds(track_x, track_w);

    // Clamp x to track bounds
    if (x < track_x) x = track_x;
    if (x > track_x + track_w) x = track_x + track_w;

    // Convert x position to value
    int16_t offset = x - track_x;
    uint16_t value = min_ + ((offset * (max_ - min_)) / track_w);

    // Clamp to range
    if (value < min_) value = min_;
    if (value > max_) value = max_;

    return value;
}

int16_t Slider::valueToX(uint16_t value) const {
    // Get track bounds
    int16_t track_x, track_w;
    getTrackBounds(track_x, track_w);

    // Convert value to x position
    if (max_ <= min_) return track_x;

    int16_t offset = (track_w * (value - min_)) / (max_ - min_);
    return track_x + offset;
}

void Slider::formatValue(char* buffer, size_t size) const {
    switch (display_mode_) {
        case DisplayMode::PERCENTAGE:
            snprintf(buffer, size, "%d%%", value_);
            break;
        case DisplayMode::TIME_MIN:
            snprintf(buffer, size, "%dm", value_);
            break;
        case DisplayMode::TIME_SEC:
            snprintf(buffer, size, "%ds", value_);
            break;
        case DisplayMode::NUMERIC:
        default:
            snprintf(buffer, size, "%d", value_);
            break;
    }
}

void Slider::getTrackBounds(int16_t& track_x, int16_t& track_w) const {
    // Layout: [−] Label: [====track====] Value [+]
    //        35px ~40px    ~150px        ~50px 35px

    int16_t dec_x, inc_x, btn_w, btn_h;
    getArrowBounds(dec_x, inc_x, btn_w, btn_h);

    int16_t label_width = strlen(label_) * 7;  // Approximate Font2 width
    int16_t label_x = dec_x + btn_w + ARROW_SPACING;

    // Track starts after label
    track_x = label_x + label_width + 10;

    // Track ends before value display
    int16_t value_x = inc_x - ARROW_SPACING;
    track_w = value_x - VALUE_DISPLAY_WIDTH - track_x;

    // Ensure minimum track width
    if (track_w < 50) track_w = 50;
}

void Slider::getArrowBounds(int16_t& dec_x, int16_t& inc_x, int16_t& btn_w, int16_t& btn_h) const {
    btn_w = ARROW_BTN_WIDTH;
    btn_h = ARROW_BTN_HEIGHT;  // Fixed height, centered vertically

    // Decrement button on left edge
    dec_x = bounds_.x;

    // Increment button on right edge
    inc_x = bounds_.x + bounds_.w - btn_w;
}

void Slider::handleIncrement() {
    if (value_ < max_) {
        setValue(value_ + 1);
        if (callback_) {
            callback_(value_);
        }
    }
}

void Slider::handleDecrement() {
    if (value_ > min_) {
        setValue(value_ - 1);
        if (callback_) {
            callback_(value_);
        }
    }
}
