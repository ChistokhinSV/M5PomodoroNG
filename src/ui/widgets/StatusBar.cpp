#include "StatusBar.h"
#include <M5Unified.h>
#include <string.h>
#include <stdio.h>

StatusBar::StatusBar()
    : battery_percent_(100),
      battery_charging_(false),
      wifi_connected_(false),
      hour_(0),
      minute_(0) {
    strcpy(power_mode_, "BAL");
}

void StatusBar::updateBattery(uint8_t percent, bool charging) {
    if (battery_percent_ != percent || battery_charging_ != charging) {
        battery_percent_ = percent;
        battery_charging_ = charging;
        markDirty();
    }
}

void StatusBar::updateWiFi(bool connected) {
    if (wifi_connected_ != connected) {
        wifi_connected_ = connected;
        markDirty();
    }
}

void StatusBar::updateMode(const char* mode) {
    if (mode && strcmp(power_mode_, mode) != 0) {
        strncpy(power_mode_, mode, sizeof(power_mode_) - 1);
        power_mode_[sizeof(power_mode_) - 1] = '\0';
        markDirty();
    }
}

void StatusBar::updateTime(uint8_t hour, uint8_t minute) {
    if (hour_ != hour || minute_ != minute) {
        hour_ = hour;
        minute_ = minute;
        markDirty();
    }
}

void StatusBar::draw(Renderer& renderer) {
    if (!visible_) return;

    // Draw background
    renderer.drawRect(bounds_.x, bounds_.y, bounds_.w, bounds_.h,
                     Renderer::Color(0x222222), true);

    // Draw separator line at bottom
    renderer.drawLine(bounds_.x, bounds_.y + bounds_.h - 1,
                     bounds_.x + bounds_.w, bounds_.y + bounds_.h - 1,
                     Renderer::Color(0x444444));

    // Layout: [WiFi] [Mode] [Time] [Battery]
    drawWiFi(renderer, bounds_.x + 5, bounds_.y + 10);
    drawMode(renderer, bounds_.x + 40, bounds_.y + 10);
    drawTime(renderer, bounds_.x + 160, bounds_.y + 10);
    drawBattery(renderer, bounds_.x + bounds_.w - 60, bounds_.y + 10);

    clearDirty();
}

void StatusBar::drawBattery(Renderer& renderer, int16_t x, int16_t y) {
    // Battery icon: [===] 100%
    // Draw battery outline (12×6px)
    renderer.drawRect(x, y - 3, 12, 6, Renderer::Color(TFT_WHITE), false);

    // Battery terminal (2px stub on right)
    renderer.drawLine(x + 12, y - 1, x + 12, y + 1, Renderer::Color(TFT_WHITE));

    // Fill level (based on percentage)
    if (battery_percent_ > 0) {
        uint8_t fill_width = (battery_percent_ * 10) / 100;
        if (fill_width > 10) fill_width = 10;

        Renderer::Color fill_color = Renderer::Color(TFT_GREEN);
        if (battery_percent_ <= 20) {
            fill_color = Renderer::Color(TFT_RED);
        } else if (battery_percent_ <= 50) {
            fill_color = Renderer::Color(TFT_YELLOW);
        }

        renderer.drawRect(x + 1, y - 2, fill_width, 4, fill_color, true);
    }

    // Charging indicator (lightning bolt or "+")
    if (battery_charging_) {
        renderer.drawString(x + 6, y, "+", &fonts::Font0, Renderer::Color(TFT_YELLOW));
    }

    // Percentage text
    char bat_str[5];
    snprintf(bat_str, sizeof(bat_str), "%d%%", battery_percent_);
    renderer.setTextDatum(ML_DATUM);  // Middle-left
    renderer.drawString(x + 16, y, bat_str, &fonts::Font0, Renderer::Color(TFT_WHITE));
}

void StatusBar::drawWiFi(Renderer& renderer, int16_t x, int16_t y) {
    // WiFi icon: ((( or X
    if (wifi_connected_) {
        // Simple arc representation: )))
        renderer.drawString(x, y, ")))", &fonts::Font0, Renderer::Color(TFT_GREEN));
    } else {
        // Disconnected: X
        renderer.drawString(x, y, "X", &fonts::Font0, Renderer::Color(TFT_RED));
    }
}

void StatusBar::drawMode(Renderer& renderer, int16_t x, int16_t y) {
    // Power mode text: BAL, PERF, or SAVER
    Renderer::Color mode_color = Renderer::Color(TFT_CYAN);  // BAL default
    if (strcmp(power_mode_, "PERF") == 0) {
        mode_color = Renderer::Color(TFT_RED);
    } else if (strcmp(power_mode_, "SAVER") == 0) {
        mode_color = Renderer::Color(TFT_GREEN);
    }

    renderer.setTextDatum(ML_DATUM);  // Middle-left
    renderer.drawString(x, y, power_mode_, &fonts::Font0, mode_color);
}

void StatusBar::drawTime(Renderer& renderer, int16_t x, int16_t y) {
    // Time: HH:MM
    char time_str[6];
    snprintf(time_str, sizeof(time_str), "%02d:%02d", hour_, minute_);

    renderer.setTextDatum(MC_DATUM);  // Middle-center
    renderer.drawString(x, y, time_str, &fonts::Font0, Renderer::Color(TFT_WHITE));
}
