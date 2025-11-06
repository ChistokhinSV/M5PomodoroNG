#ifndef STATSSCREEN_H
#define STATSSCREEN_H

#include <functional>
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
class StatsScreen {
public:
    StatsScreen(Statistics& statistics, NavigationCallback navigate_callback);

    // Lifecycle
    void draw(Renderer& renderer);
    void update(uint32_t deltaMs);
    void handleTouch(int16_t x, int16_t y, bool pressed);

    // Configuration
    void updateStatus(uint8_t battery, bool charging, bool wifi, const char* mode, uint8_t hour, uint8_t minute);
    void markDirty() { needs_redraw_ = true; }

    // Hardware button interface
    void getButtonLabels(const char*& btnA, const char*& btnB, const char*& btnC);
    void onButtonA();  // Back to Main
    void onButtonB();  // (unused)
    void onButtonC();  // (unused)

private:
    Statistics& statistics_;
    NavigationCallback navigate_callback_;

    // Widgets
    StatusBar status_bar_;
    StatsChart stats_chart_;
    // Note: Back button removed, now using hardware button

    // State
    bool needs_redraw_;

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
    void drawSummary(Renderer& renderer);       // Today + Streak
    void drawChartTitle(Renderer& renderer);
    void drawLifetimeStats(Renderer& renderer); // Total + Avg
};

#endif // STATSSCREEN_H
