#ifndef STATSSCREEN_H
#define STATSSCREEN_H

#include <functional>
#include "../Screen.h"
#include "../Renderer.h"
#include "../widgets/StatusBar.h"
#include "../widgets/StatsChart.h"
#include "../widgets/Button.h"
#include "../../core/Statistics.h"

// Forward declare ScreenID from ScreenManager.h (avoid circular include)
enum class ScreenID;
using NavigationCallback = std::function<void(ScreenID)>;

/**
 * Statistics screen - displays productivity data and weekly chart
 *
 * Layout (320×240):
 * ┌─────────────────────────────────┐
 * │ [WiFi][Mode][Time][Battery]     │ ← StatusBar (20px)
 * ├─────────────────────────────────┤
 * │ [← Back]      Statistics         │ ← Title + Back button (30px)
 * ├─────────────────────────────────┤
 * │ Today: 6🍅    Streak: 3 days    │ ← Summary stats (30px)
 * ├─────────────────────────────────┤
 * │ This Week                        │ ← Chart title (20px)
 * │  ┌────────────────────────┐     │
 * │  │ █ █ █ █ █ ░ ░          │     │ ← StatsChart widget (100px)
 * │  │ M T W T F S S          │     │
 * │  └────────────────────────┘     │
 * ├─────────────────────────────────┤
 * │ Total: 847🍅  Avg: 6.2/day      │ ← Lifetime stats (20px)
 * └─────────────────────────────────┘
 *
 * Features:
 * - Weekly bar chart showing Mon-Sun session counts
 * - Today's completed sessions
 * - Current streak (consecutive days)
 * - Lifetime total and 7-day average
 * - Back button to return to main screen
 */
class StatsScreen : public Screen {
public:
    StatsScreen(Statistics& statistics, NavigationCallback navigate_callback);

    // Override Screen interface
    void draw(Renderer& renderer) override;
    void update(uint32_t deltaMs) override;
    void updateStatus(uint8_t battery, bool charging, bool wifi, const char* mode, uint8_t hour, uint8_t minute) override;
    void getButtonLabels(const char*& btnA, const char*& btnB, const char*& btnC) override;
    void onButtonA() override;  // Back to Main  (or Cancel while reset dialog open)
    void onButtonB() override;  // (unused)
    void onButtonC() override;  // Long-press: stats reset dialog. Confirm Yes while dialog open.
    void handleTouch(int16_t x, int16_t y, bool pressed) override;

private:
    Statistics& statistics_;
    NavigationCallback navigate_callback_;

    // Widgets
    StatusBar status_bar_;
    StatsChart stats_chart_;
    // Note: Back button removed, now using hardware button
    // Note: needs_redraw_ inherited from Screen base class

    // Layout constants
    static constexpr int16_t SCREEN_WIDTH = 320;
    static constexpr int16_t SCREEN_HEIGHT = 240;
    static constexpr int16_t STATUS_BAR_HEIGHT = 20;
    static constexpr int16_t TITLE_HEIGHT = 30;
    static constexpr int16_t SUMMARY_HEIGHT = 30;
    static constexpr int16_t CHART_TITLE_HEIGHT = 20;
    static constexpr int16_t CHART_HEIGHT = 100;
    static constexpr int16_t LIFETIME_HEIGHT = 20;

    // Drawing helpers
    void drawTitle(Renderer& renderer);
    void drawSummary(Renderer& renderer);       // Today + Week
    void drawChartTitle(Renderer& renderer);
    void drawLifetimeStats(Renderer& renderer); // Total + Streak

    // --- BtnC long-press → reset all statistics ---
    // Mirrors the BtnA hold pattern on MainScreen. Hold BtnC for 1.5s to open
    // a confirm dialog; tap Yes (on-screen or BtnC) to wipe the NVS stats.
    uint32_t btn_c_press_start_ = 0;     // 0 = not held; else millis() at press
    bool btn_c_long_press_consumed_ = false;
    bool reset_dialog_visible_ = false;
    static constexpr uint32_t LONG_PRESS_MS = 1500;

    // Reset-dialog layout (matches MainScreen sizing for consistency)
    static constexpr int16_t DIALOG_W = 240;
    static constexpr int16_t DIALOG_H = 110;
    static constexpr int16_t DIALOG_BTN_W = 90;
    static constexpr int16_t DIALOG_BTN_H = 32;

    void drawLongPressProgress(Renderer& renderer);
    void drawResetDialog(Renderer& renderer);
    void performStatsReset();
};

#endif // STATSSCREEN_H
