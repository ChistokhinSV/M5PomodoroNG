#include "StatsScreen.h"
#include <M5Unified.h>
#include <stdio.h>

// Static instance pointer for button callbacks
StatsScreen* StatsScreen::instance_ = nullptr;

StatsScreen::StatsScreen(Statistics& statistics)
    : statistics_(statistics),
      needs_redraw_(true) {

    // Set static instance for callbacks
    instance_ = this;

    // Configure widgets with layout positions
    // Status bar at top (320×20)
    status_bar_.setBounds(0, 0, SCREEN_WIDTH, STATUS_BAR_HEIGHT);

    // Back button (top-left, below status bar)
    btn_back_.setBounds(10, STATUS_BAR_HEIGHT + 5, 70, 20);
    btn_back_.setLabel("<- Back");
    btn_back_.setCallback(onBackPress);

    // Stats chart in middle
    int16_t chart_y = STATUS_BAR_HEIGHT + TITLE_HEIGHT + SUMMARY_HEIGHT + CHART_TITLE_HEIGHT;
    stats_chart_.setBounds(20, chart_y, 280, CHART_HEIGHT);

    // Load weekly data into chart
    Statistics::DayStats last7[7];
    statistics_.getLast7Days(last7);

    // Convert to simple counts for chart (last 7 days, 0 = oldest, 6 = today)
    uint8_t weekly_data[7];
    for (uint8_t i = 0; i < 7; i++) {
        weekly_data[i] = last7[i].completed_sessions;
    }

    stats_chart_.setData(weekly_data);
    stats_chart_.setMaxValue(0);  // Auto-scale
}

void StatsScreen::updateStatus(uint8_t battery, bool charging, bool wifi,
                                const char* mode, uint8_t hour, uint8_t minute) {
    status_bar_.updateBattery(battery, charging);
    status_bar_.updateWiFi(wifi);
    status_bar_.updateMode(mode);
    status_bar_.updateTime(hour, minute);
}

void StatsScreen::update(uint32_t deltaMs) {
    // Reload stats data (in case it changed)
    Statistics::DayStats last7[7];
    statistics_.getLast7Days(last7);

    uint8_t weekly_data[7];
    for (uint8_t i = 0; i < 7; i++) {
        weekly_data[i] = last7[i].completed_sessions;
    }
    stats_chart_.setData(weekly_data);

    needs_redraw_ = true;
}

void StatsScreen::draw(Renderer& renderer) {
    if (!needs_redraw_) return;

    // Clear background
    renderer.clear(Renderer::Color(TFT_BLACK));

    // Draw status bar at top
    status_bar_.draw(renderer);

    // Draw title and back button
    drawTitle(renderer);
    btn_back_.draw(renderer);

    // Draw summary (Today + Streak)
    drawSummary(renderer);

    // Draw chart title
    drawChartTitle(renderer);

    // Draw weekly stats chart
    stats_chart_.draw(renderer);

    // Draw lifetime stats (Total + Avg)
    drawLifetimeStats(renderer);

    needs_redraw_ = false;
}

void StatsScreen::handleTouch(int16_t x, int16_t y, bool pressed) {
    if (pressed) {
        // Touch down - check button hit
        if (btn_back_.hitTest(x, y)) {
            btn_back_.onTouch(x, y);
        }
    } else {
        // Touch up - release button
        btn_back_.onRelease(x, y);
    }
}

void StatsScreen::drawTitle(Renderer& renderer) {
    // Draw "Statistics" title centered
    int16_t y = STATUS_BAR_HEIGHT + 15;
    renderer.setTextDatum(TC_DATUM);  // Top-center
    renderer.drawString(SCREEN_WIDTH / 2, y, "Statistics",
                       &fonts::Font4, Renderer::Color(TFT_CYAN));
}

void StatsScreen::drawSummary(Renderer& renderer) {
    // Draw Today and Streak stats
    int16_t y = STATUS_BAR_HEIGHT + TITLE_HEIGHT + 15;

    // Today's count (left side)
    char today_str[16];
    Statistics::DayStats today = statistics_.getToday();
    snprintf(today_str, sizeof(today_str), "Today: %d", today.completed_sessions);

    renderer.setTextDatum(TL_DATUM);  // Top-left
    renderer.drawString(40, y, today_str,
                       &fonts::Font2, Renderer::Color(TFT_WHITE));

    // Streak (right side) - calculate from last 7 days
    char streak_str[20];
    Statistics::DayStats last7[7];
    statistics_.getLast7Days(last7);

    // Count consecutive days with at least 1 session (from today backwards)
    uint16_t streak = 0;
    for (int8_t i = 6; i >= 0; i--) {  // 6 = today, 0 = 7 days ago
        if (last7[i].completed_sessions > 0) {
            streak++;
        } else {
            break;  // Streak broken
        }
    }

    if (streak == 1) {
        snprintf(streak_str, sizeof(streak_str), "Streak: 1 day");
    } else {
        snprintf(streak_str, sizeof(streak_str), "Streak: %d days", streak);
    }

    renderer.setTextDatum(TR_DATUM);  // Top-right
    renderer.drawString(SCREEN_WIDTH - 40, y, streak_str,
                       &fonts::Font2, Renderer::Color(TFT_GREEN));
}

void StatsScreen::drawChartTitle(Renderer& renderer) {
    // Draw "This Week" label above chart
    int16_t y = STATUS_BAR_HEIGHT + TITLE_HEIGHT + SUMMARY_HEIGHT + 10;

    renderer.setTextDatum(TL_DATUM);  // Top-left
    renderer.drawString(20, y, "This Week",
                       &fonts::Font2, Renderer::Color(TFT_LIGHTGRAY));
}

void StatsScreen::drawLifetimeStats(Renderer& renderer) {
    // Draw Total and Average stats at bottom
    int16_t y = SCREEN_HEIGHT - LIFETIME_HEIGHT - 5;

    // Total (left side)
    char total_str[20];
    uint16_t total = statistics_.getTotalCompleted();
    snprintf(total_str, sizeof(total_str), "Total: %d", total);

    renderer.setTextDatum(BL_DATUM);  // Bottom-left
    renderer.drawString(40, y, total_str,
                       &fonts::Font2, Renderer::Color(TFT_WHITE));

    // 7-day average (right side)
    char avg_str[20];
    uint16_t week_total = statistics_.getLast7DaysTotal();
    float avg = week_total / 7.0f;

    snprintf(avg_str, sizeof(avg_str), "Avg: %.1f/day", avg);

    renderer.setTextDatum(BR_DATUM);  // Bottom-right
    renderer.drawString(SCREEN_WIDTH - 40, y, avg_str,
                       &fonts::Font2, Renderer::Color(TFT_CYAN));
}

// Button callback implementation
void StatsScreen::onBackPress() {
    if (!instance_) return;

    // TODO: Navigate back to main screen (awaits MP-21 Screen Manager)
    Serial.println("[StatsScreen] Back button pressed (not implemented yet)");

    instance_->needs_redraw_ = true;
}
