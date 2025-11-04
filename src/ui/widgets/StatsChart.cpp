#include "StatsChart.h"
#include <M5Unified.h>
#include <string.h>
#include <stdio.h>

StatsChart::StatsChart()
    : max_value_(0),
      auto_scale_(true) {
    memset(data_, 0, sizeof(data_));
}

void StatsChart::setData(const uint8_t data[7]) {
    bool changed = false;
    for (uint8_t i = 0; i < 7; i++) {
        if (data_[i] != data[i]) {
            data_[i] = data[i];
            changed = true;
        }
    }

    if (changed) {
        markDirty();
    }
}

void StatsChart::setMaxValue(uint8_t max) {
    if (max == 0) {
        auto_scale_ = true;
    } else {
        auto_scale_ = false;
        max_value_ = max;
    }
    markDirty();
}

void StatsChart::draw(Renderer& renderer) {
    if (!visible_) return;

    // Calculate max value for scaling
    uint8_t display_max = max_value_;
    if (auto_scale_) {
        display_max = 0;
        for (uint8_t i = 0; i < 7; i++) {
            if (data_[i] > display_max) {
                display_max = data_[i];
            }
        }
        if (display_max == 0) display_max = 1;  // Avoid division by zero
    }

    // Calculate layout
    const uint8_t BAR_SPACING = 5;
    const uint8_t LABEL_HEIGHT = 12;
    const uint8_t VALUE_HEIGHT = 12;
    const uint8_t MARGIN = 10;

    int16_t chart_height = bounds_.h - LABEL_HEIGHT - VALUE_HEIGHT - MARGIN * 2;
    int16_t bar_width = (bounds_.w - (6 * BAR_SPACING) - MARGIN * 2) / 7;

    // Draw title/legend area
    renderer.drawString(bounds_.x + bounds_.w / 2, bounds_.y + 10,
                       "Weekly Stats", &fonts::Font2,
                       Renderer::Color(TFT_WHITE));

    // Draw bars
    int16_t x = bounds_.x + MARGIN;
    int16_t baseline_y = bounds_.y + bounds_.h - LABEL_HEIGHT - MARGIN;

    for (uint8_t i = 0; i < 7; i++) {
        // Calculate bar height
        int16_t bar_height = 0;
        if (data_[i] > 0 && display_max > 0) {
            bar_height = (data_[i] * chart_height) / display_max;
            if (bar_height < 2 && data_[i] > 0) {
                bar_height = 2;  // Minimum visible height
            }
        }

        // Draw bar
        if (bar_height > 0) {
            // Color gradient: red to green based on value
            Renderer::Color bar_color = Renderer::Color(TFT_GREEN);
            if (data_[i] < display_max / 3) {
                bar_color = Renderer::Color(TFT_RED);
            } else if (data_[i] < 2 * display_max / 3) {
                bar_color = Renderer::Color(TFT_YELLOW);
            }

            int16_t bar_y = baseline_y - bar_height;
            renderer.drawRect(x, bar_y, bar_width, bar_height, bar_color, true);

            // Draw border
            renderer.drawRect(x, bar_y, bar_width, bar_height,
                             Renderer::Color(TFT_WHITE), false);

            // Draw value on top of bar
            if (bar_height > 15) {  // Only if bar is tall enough
                char value_str[4];
                snprintf(value_str, sizeof(value_str), "%d", data_[i]);
                renderer.setTextDatum(MC_DATUM);
                renderer.drawString(x + bar_width / 2, bar_y + 8, value_str,
                                   &fonts::Font0, Renderer::Color(TFT_BLACK));
            }
        }

        // Draw day label below bar
        renderer.setTextDatum(MC_DATUM);
        renderer.drawString(x + bar_width / 2, baseline_y + 8,
                           DAY_LABELS[i], &fonts::Font0,
                           Renderer::Color(TFT_LIGHTGRAY));

        x += bar_width + BAR_SPACING;
    }

    clearDirty();
}
