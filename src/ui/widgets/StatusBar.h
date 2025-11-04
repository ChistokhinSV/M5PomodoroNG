#ifndef STATUSBAR_H
#define STATUSBAR_H

#include "Widget.h"

/**
 * Status bar widget for top of screen
 *
 * Displays:
 * - Battery percentage + charging indicator (right side)
 * - WiFi connection status (left side)
 * - Power mode: BAL/PERF/SAVER (center-left)
 * - Current time HH:MM (center-right)
 *
 * Updates:
 * - Battery: Every 30 seconds
 * - WiFi: On connection state change
 * - Mode: On power mode change
 * - Time: Every minute
 *
 * Typical height: 20px
 * Width: Full screen (320px)
 */
class StatusBar : public Widget {
public:
    StatusBar();

    // Update methods
    void updateBattery(uint8_t percent, bool charging);
    void updateWiFi(bool connected);
    void updateMode(const char* mode);  // "BAL", "PERF", "SAVER"
    void updateTime(uint8_t hour, uint8_t minute);

    // Widget interface
    void draw(Renderer& renderer) override;

private:
    uint8_t battery_percent_;
    bool battery_charging_;
    bool wifi_connected_;
    char power_mode_[6];  // "SAVER" + null
    uint8_t hour_;
    uint8_t minute_;

    // Draw individual components
    void drawBattery(Renderer& renderer, int16_t x, int16_t y);
    void drawWiFi(Renderer& renderer, int16_t x, int16_t y);
    void drawMode(Renderer& renderer, int16_t x, int16_t y);
    void drawTime(Renderer& renderer, int16_t x, int16_t y);
};

#endif // STATUSBAR_H
