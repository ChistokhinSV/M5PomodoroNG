#include "StatsScreen.h"
#include "../ScreenManager.h"
#include "../../core/TimeManager.h"
#include <M5Unified.h>
#include <stdio.h>

extern TimeManager* g_timeManager;

StatsScreen::StatsScreen(Statistics& statistics, NavigationCallback navigate_callback)
    : statistics_(statistics),
      navigate_callback_(navigate_callback) {
    // Note: needs_redraw_ inherited from Screen base class

    // Configure widgets with layout positions
    // Status bar at top (320×20)
    status_bar_.setBounds(0, 0, SCREEN_WIDTH, STATUS_BAR_HEIGHT);

    // Note: Back button removed, now using hardware button (BtnA)

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
    if (g_timeManager) {
        stats_chart_.setTodayWeekday(g_timeManager->getCurrentWeekday());
    }
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

    // Refresh weekday alignment in case the screen sits open across midnight.
    if (g_timeManager) {
        stats_chart_.setTodayWeekday(g_timeManager->getCurrentWeekday());
    }

    // BtnC long-press → stats reset dialog. onButtonC() seeded btn_c_press_start_
    // on wasPressed. Cross the threshold while still held → open the dialog.
    // Short release → no action (BtnC has no short-press behavior on this screen).
    if (btn_c_press_start_ != 0) {
        if (M5.BtnC.isPressed()) {
            if (!btn_c_long_press_consumed_ &&
                !reset_dialog_visible_ &&
                millis() - btn_c_press_start_ >= LONG_PRESS_MS) {
                btn_c_long_press_consumed_ = true;
                reset_dialog_visible_ = true;
                Serial.println("[StatsScreen] BtnC long-press → stats reset dialog");
            }
        } else {
            btn_c_press_start_ = 0;
            btn_c_long_press_consumed_ = false;
        }
    }

    needs_redraw_ = true;
}

void StatsScreen::draw(Renderer& renderer) {
    if (!needs_redraw_) return;

    // Clear background
    renderer.clear(Renderer::Color(TFT_BLACK));

    // Draw status bar at top
    status_bar_.draw(renderer);

    // Draw title (back button now in hardware button bar)
    drawTitle(renderer);

    // Draw summary (Today + Streak)
    drawSummary(renderer);

    // Draw chart title
    drawChartTitle(renderer);

    // Draw weekly stats chart
    stats_chart_.draw(renderer);

    // Draw lifetime stats (Total + Streak)
    drawLifetimeStats(renderer);

    // Hold-progress bar (only visible while a BtnC hold is in flight)
    drawLongPressProgress(renderer);

    // Reset confirmation dialog goes on top of everything else
    if (reset_dialog_visible_) {
        drawResetDialog(renderer);
    }

    needs_redraw_ = false;
}

void StatsScreen::handleTouch(int16_t x, int16_t y, bool pressed) {
    // While the confirm dialog is open, hit-test Yes/No and swallow everything
    // else so the chart/widgets underneath don't react.
    if (reset_dialog_visible_) {
        if (pressed) {
            int16_t dx = (SCREEN_WIDTH - DIALOG_W) / 2;
            int16_t dy = (SCREEN_HEIGHT - DIALOG_H) / 2;
            int16_t btn_y = dy + DIALOG_H - DIALOG_BTN_H - 10;
            int16_t no_x = dx + 20;
            int16_t yes_x = dx + DIALOG_W - DIALOG_BTN_W - 20;

            if (y >= btn_y && y <= btn_y + DIALOG_BTN_H) {
                if (x >= no_x && x <= no_x + DIALOG_BTN_W) {
                    Serial.println("[StatsScreen] Reset dialog: No");
                    reset_dialog_visible_ = false;
                    needs_redraw_ = true;
                    return;
                }
                if (x >= yes_x && x <= yes_x + DIALOG_BTN_W) {
                    Serial.println("[StatsScreen] Reset dialog: Yes");
                    performStatsReset();
                    reset_dialog_visible_ = false;
                    needs_redraw_ = true;
                    return;
                }
            }
        }
        return;  // Swallow everything else
    }

    // Otherwise defer to base for widget event dispatch
    Screen::handleTouch(x, y, pressed);
}

void StatsScreen::drawTitle(Renderer& renderer) {
    // Draw "Statistics" title centered
    int16_t y = STATUS_BAR_HEIGHT + 15;
    renderer.setTextDatum(TC_DATUM);  // Top-center
    renderer.drawString(SCREEN_WIDTH / 2, y, "Statistics",
                       &fonts::Font4, Renderer::Color(TFT_CYAN));
}

void StatsScreen::drawSummary(Renderer& renderer) {
    // Top row: Today (left) · Week (right)
    int16_t y = STATUS_BAR_HEIGHT + TITLE_HEIGHT + 15;

    // Today's count (left side)
    char today_str[16];
    Statistics::DayStats today = statistics_.getToday();
    snprintf(today_str, sizeof(today_str), "Today: %u", today.completed_sessions);

    renderer.setTextDatum(TL_DATUM);  // Top-left
    renderer.drawString(40, y, today_str,
                       &fonts::Font2, Renderer::Color(TFT_WHITE));

    // Rolling 7-day total (right side)
    char week_str[20];
    snprintf(week_str, sizeof(week_str), "Week: %u",
             statistics_.getLast7DaysTotal());

    renderer.setTextDatum(TR_DATUM);  // Top-right
    renderer.drawString(SCREEN_WIDTH - 40, y, week_str,
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
    // Bottom row: Total (left) · Streak (right)
    int16_t y = SCREEN_HEIGHT - LIFETIME_HEIGHT - 5;

    // Lifetime total (left side)
    char total_str[20];
    snprintf(total_str, sizeof(total_str), "Total: %u",
             statistics_.getTotalCompleted());

    renderer.setTextDatum(BL_DATUM);  // Bottom-left
    renderer.drawString(40, y, total_str,
                       &fonts::Font2, Renderer::Color(TFT_WHITE));

    // Streak = consecutive recent days with >=1 completed work session,
    // counted backwards from today (last7[6] = today, last7[0] = 7 days ago).
    Statistics::DayStats last7[7];
    statistics_.getLast7Days(last7);
    uint16_t streak = 0;
    for (int8_t i = 6; i >= 0; i--) {
        if (last7[i].completed_sessions > 0) {
            streak++;
        } else {
            break;
        }
    }

    char streak_str[20];
    snprintf(streak_str, sizeof(streak_str),
             streak == 1 ? "Streak: 1 day" : "Streak: %u days", streak);

    renderer.setTextDatum(BR_DATUM);  // Bottom-right
    renderer.drawString(SCREEN_WIDTH - 40, y, streak_str,
                       &fonts::Font2, Renderer::Color(TFT_CYAN));
}

// Hardware button interface implementation
void StatsScreen::getButtonLabels(const char*& btnA, const char*& btnB, const char*& btnC) {
    if (reset_dialog_visible_) {
        btnA = "No";       // Cancel
        btnB = "";
        btnC = "Yes";      // Confirm
        return;
    }
    btnA = "<- Back";      // Navigate to MainScreen
    btnB = "";
    btnC = "Reset";        // Hold 1.5s to open reset dialog
}

void StatsScreen::onButtonA() {
    // While the reset confirm is open, BtnA cancels (mirrors MainScreen UX).
    if (reset_dialog_visible_) {
        Serial.println("[StatsScreen] BtnA: Cancel reset dialog");
        reset_dialog_visible_ = false;
        needs_redraw_ = true;
        return;
    }
    // Otherwise: navigate back to MainScreen
    Serial.println("[StatsScreen] BtnA: Navigate to Main");
    if (navigate_callback_) {
        navigate_callback_(ScreenID::MAIN);
    }
}

void StatsScreen::onButtonB() {
    // Inert (including while dialog is open).
}

void StatsScreen::onButtonC() {
    if (reset_dialog_visible_) {
        Serial.println("[StatsScreen] BtnC: Confirm stats reset");
        performStatsReset();
        reset_dialog_visible_ = false;
        needs_redraw_ = true;
        return;
    }
    // Otherwise seed the hold timer. update() watches BtnC.isPressed() and
    // pops the dialog once LONG_PRESS_MS has elapsed.
    btn_c_press_start_ = millis();
    btn_c_long_press_consumed_ = false;
}

void StatsScreen::drawLongPressProgress(Renderer& renderer) {
    // Thin growing bar across the top edge while BtnC is being held.
    if (btn_c_press_start_ == 0) return;
    uint32_t held = millis() - btn_c_press_start_;
    if (held < 200) return;  // Grace period so quick taps don't flash

    uint32_t clamped = held > LONG_PRESS_MS ? LONG_PRESS_MS : held;
    int16_t fill_w = (int16_t)((SCREEN_WIDTH * clamped) / LONG_PRESS_MS);
    int16_t y = STATUS_BAR_HEIGHT;
    renderer.drawRect(0, y, SCREEN_WIDTH, 3, Renderer::Color(TFT_DARKGREY), true);
    renderer.drawRect(0, y, fill_w, 3, Renderer::Color(TFT_ORANGE), true);
}

void StatsScreen::drawResetDialog(Renderer& renderer) {
    int16_t dx = (SCREEN_WIDTH - DIALOG_W) / 2;
    int16_t dy = (SCREEN_HEIGHT - DIALOG_H) / 2;

    // Dim the screen behind the dialog so it reads as modal.
    renderer.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Renderer::Color(TFT_BLACK), true);

    // Dialog box
    renderer.drawRect(dx, dy, DIALOG_W, DIALOG_H, Renderer::Color(TFT_NAVY), true);
    renderer.drawRect(dx, dy, DIALOG_W, DIALOG_H, Renderer::Color(TFT_WHITE), false);

    renderer.setTextDatum(TC_DATUM);
    renderer.drawString(SCREEN_WIDTH / 2, dy + 12, "Reset stats?",
                        &fonts::Font4, Renderer::Color(TFT_WHITE));
    renderer.drawString(SCREEN_WIDTH / 2, dy + 42, "Wipes all daily history",
                        &fonts::Font2, Renderer::Color(TFT_LIGHTGRAY));

    int16_t btn_y = dy + DIALOG_H - DIALOG_BTN_H - 10;
    int16_t no_x = dx + 20;
    int16_t yes_x = dx + DIALOG_W - DIALOG_BTN_W - 20;

    renderer.drawRect(no_x, btn_y, DIALOG_BTN_W, DIALOG_BTN_H, Renderer::Color(TFT_DARKGREY), true);
    renderer.drawRect(no_x, btn_y, DIALOG_BTN_W, DIALOG_BTN_H, Renderer::Color(TFT_WHITE), false);
    renderer.setTextDatum(MC_DATUM);
    renderer.drawString(no_x + DIALOG_BTN_W / 2, btn_y + DIALOG_BTN_H / 2, "No",
                        &fonts::Font4, Renderer::Color(TFT_WHITE));

    renderer.drawRect(yes_x, btn_y, DIALOG_BTN_W, DIALOG_BTN_H, Renderer::Color(TFT_RED), true);
    renderer.drawRect(yes_x, btn_y, DIALOG_BTN_W, DIALOG_BTN_H, Renderer::Color(TFT_WHITE), false);
    renderer.drawString(yes_x + DIALOG_BTN_W / 2, btn_y + DIALOG_BTN_H / 2, "Yes",
                        &fonts::Font4, Renderer::Color(TFT_WHITE));
}

void StatsScreen::performStatsReset() {
    // Wipe the entire NVS stats namespace (90-day buffer + overflow). The next
    // recordWorkSession() will re-create today's slot at zero.
    statistics_.clear();
    Serial.println("[StatsScreen] Statistics reset: NVS cleared");
}
