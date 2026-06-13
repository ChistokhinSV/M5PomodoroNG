#include "MainScreen.h"
#include "../ScreenManager.h"
#include <M5Unified.h>
#include <stdio.h>
#include <string.h>

// MP-27: External LED controller for pattern testing
extern ILEDController* g_ledController;

MainScreen::MainScreen(TimerStateMachine& state_machine,
                       PomodoroSequence& sequence,
                       Statistics& statistics,
                       NavigationCallback navigate_callback)
    : state_machine_(state_machine),
      sequence_(sequence),
      statistics_(statistics),
      navigate_callback_(navigate_callback),
      last_update_ms_(0) {
    // Note: needs_redraw_ inherited from Screen base class, initialized to false

    TIMER_HEIGHT = uint16_t(TIMER_FONT.height);

    strcpy(task_name_, "Focus Session");

    // Configure widgets with layout positions
    // Status bar at top (320×20)
    status_bar_.setBounds(0, 0, SCREEN_WIDTH, STATUS_BAR_HEIGHT);

    // Sequence indicator below status bar (200×20, centered, below mode label)
    sequence_indicator_.setBounds(60, STATUS_BAR_HEIGHT + MODE_LABEL_HEIGHT + 5, 200, SEQUENCE_HEIGHT);
    sequence_indicator_.setDotsPerGroup(4);  // Classic mode default

    // Progress bar in middle (280×20, centered)
    int16_t progress_y = STATUS_BAR_HEIGHT + MODE_LABEL_HEIGHT + SEQUENCE_HEIGHT + TIMER_HEIGHT + TIMER_GAP * 2;
    progress_bar_.setBounds(20, progress_y, 280, PROGRESS_HEIGHT);
    progress_bar_.setShowPercentage(false);  // Don't show percentage on timer progress
    progress_bar_.setColor(Renderer::Color(TFT_RED));

    // Note: Hardware buttons replaced custom touch buttons
    // BtnA (left): Start/Pause (state-dependent)
    // BtnB (center): Stats
    // BtnC (right): Settings
}

void MainScreen::setTaskName(const char* task) {
    if (task) {
        strncpy(task_name_, task, sizeof(task_name_) - 1);
        task_name_[sizeof(task_name_) - 1] = '\0';
        needs_redraw_ = true;
    }
}

void MainScreen::updateStatus(uint8_t battery, bool charging, bool wifi,
                               const char* mode, uint8_t hour, uint8_t minute) {
    status_bar_.updateBattery(battery, charging);
    status_bar_.updateWiFi(wifi);
    status_bar_.updateMode(mode);
    status_bar_.updateTime(hour, minute);
}

void MainScreen::update(uint32_t deltaMs) {
    // Update state machine (MP-23: also updates LEDs via TimerStateMachine)
    state_machine_.update(deltaMs);

    // Update progress bar (0% at start, 100% at end)
    uint8_t progress = state_machine_.getProgressPercent();
    progress_bar_.setProgress(progress);

    // Update sequence indicator
    auto session = sequence_.getCurrentSession();

    // Each session (group) contains 4 work intervals
    // Example: 3 sessions = 12 dots total, displayed as: ●●●● ●●●● ●●●●
    // Large spacing between groups = long break
    // Small spacing within groups = short breaks
    // MP-51: Show work session progress with cycle grouping
    sequence_indicator_.setTotalSessions(sequence_.getTotalWorkSessions());
    sequence_indicator_.setDotsPerGroup(sequence_.getSessionsBeforeLong());  // Sessions per cycle

    // getCurrentWorkSession() is 1-based within the current mega-cycle and
    // already wraps correctly when advance() resets current_session.
    // completed_today counts mega-cycle wraps, NOT work-session completions, so
    // `completed_today % total_work_sessions` is unit-mismatched garbage that
    // happens to be 0 whenever you've wrapped exactly N times — which left the
    // previous 3 dots gray after a clean 4-session cycle. Drive both fields
    // from the work-session counter and ignore completed_today here.
    uint8_t work_session_1b = sequence_.getCurrentWorkSession();
    uint8_t current_work_session = work_session_1b - 1;  // 0-based pulse pos
    bool in_break = !sequence_.isWorkSession();
    uint8_t completed_sessions = in_break ? work_session_1b
                                          : (work_session_1b - 1);

    // DEBUG: Log breadcrumb calculation (every 60 frames ~2 sec)
    static uint8_t breadcrumb_log_counter = 0;
    if (++breadcrumb_log_counter >= 60) {
        breadcrumb_log_counter = 0;
        Serial.printf("[BREADCRUMB DEBUG] work_session_1b=%d, total_work=%d, completed_today=%d (mega-cycle wraps)\n",
                     work_session_1b, sequence_.getTotalWorkSessions(),
                     sequence_.getCompletedToday());
        Serial.printf("[BREADCRUMB DEBUG] current_work_session=%d, completed_sessions=%d, in_break=%s\n",
                     current_work_session, completed_sessions, in_break ? "YES" : "NO");
    }

    sequence_indicator_.setSession(current_work_session,
                                   completed_sessions,
                                   in_break);

    // Update button visibility based on state
    updateButtons();

    // Update widget animations
    progress_bar_.update(deltaMs);
    sequence_indicator_.update(deltaMs);

    // Today-counter change detection. Catches session completion (count goes up)
    // and midnight rollover (count drops to 0 — Statistics::getToday rotates the
    // cache when epoch_days changes). draw() only runs when needs_redraw_ is set.
    uint16_t today_now = statistics_.getToday().completed_sessions;
    if (today_now != last_displayed_today_count_) {
        last_displayed_today_count_ = today_now;
        needs_redraw_ = true;
    }

    // --- BtnA long-press detection (IDLE state) ---
    // onButtonA() seeded btn_a_press_start_ on wasPressed. Here we watch the
    // ongoing hold to trigger the dialog at the threshold, and to fall back to
    // a normal Start if the button is released early.
    if (btn_a_press_start_ != 0) {
        if (M5.BtnA.isPressed()) {
            // Still holding — fire the dialog as soon as we cross the threshold,
            // so the user gets immediate feedback without waiting for release.
            if (!btn_a_long_press_consumed_ &&
                millis() - btn_a_press_start_ >= LONG_PRESS_MS) {
                btn_a_long_press_consumed_ = true;
                reset_dialog_visible_ = true;
                Serial.println("[MainScreen] BtnA long-press → reset dialog");
            }
        } else {
            // Released. If the threshold wasn't crossed, treat it as a normal
            // Start. Otherwise the dialog is already up — do nothing.
            uint32_t held = millis() - btn_a_press_start_;
            btn_a_press_start_ = 0;
            if (!btn_a_long_press_consumed_ && held < LONG_PRESS_MS) {
                Serial.printf("[MainScreen] BtnA short press (%lu ms) → Start\n", held);
                state_machine_.handleEvent(TimerStateMachine::Event::START);
            }
            btn_a_long_press_consumed_ = false;
        }
    }

    // --- Timer touch long-press detection ---
    // The press/release events come through handleTouch(); here we only need to
    // fire the dialog the moment we cross the threshold while still held.
    if (timer_touch_press_start_ != 0 &&
        !timer_touch_long_press_consumed_ &&
        millis() - timer_touch_press_start_ >= LONG_PRESS_MS) {
        timer_touch_long_press_consumed_ = true;
        reset_dialog_visible_ = true;
        Serial.println("[MainScreen] Timer touch long-press → reset dialog");
    }

    needs_redraw_ = true;
}

void MainScreen::handleTouch(int16_t x, int16_t y, bool pressed) {
    // If the dialog is open, take touches for Yes/No first — don't let them
    // bleed through to the timer / buttons underneath.
    if (reset_dialog_visible_) {
        if (pressed) {
            int16_t dx = (SCREEN_WIDTH - DIALOG_W) / 2;
            int16_t dy = (SCREEN_HEIGHT - DIALOG_H) / 2;
            int16_t btn_y = dy + DIALOG_H - DIALOG_BTN_H - 10;
            // No (left)
            int16_t no_x = dx + 20;
            // Yes (right)
            int16_t yes_x = dx + DIALOG_W - DIALOG_BTN_W - 20;

            if (y >= btn_y && y <= btn_y + DIALOG_BTN_H) {
                if (x >= no_x && x <= no_x + DIALOG_BTN_W) {
                    Serial.println("[MainScreen] Reset dialog: No");
                    reset_dialog_visible_ = false;
                    needs_redraw_ = true;
                    return;
                }
                if (x >= yes_x && x <= yes_x + DIALOG_BTN_W) {
                    Serial.println("[MainScreen] Reset dialog: Yes");
                    performCycleReset();
                    reset_dialog_visible_ = false;
                    needs_redraw_ = true;
                    return;
                }
            }
        }
        // Eat the touch so widgets don't react while dialog is up.
        return;
    }

    // Long-press tracking on the timer digits: starts on press inside the
    // hitbox, cancels if the finger releases or drifts out.
    if (pressed) {
        if (timer_touch_press_start_ == 0 && isInTimerHitbox(x, y)) {
            timer_touch_press_start_ = millis();
            timer_touch_long_press_consumed_ = false;
        } else if (timer_touch_press_start_ != 0 && !isInTimerHitbox(x, y)) {
            // Drag-off cancels.
            timer_touch_press_start_ = 0;
            timer_touch_long_press_consumed_ = false;
        }
    } else {
        // Release ends the hold; if threshold wasn't crossed it was just a tap
        // (no action — main screen has no short-tap timer behavior).
        timer_touch_press_start_ = 0;
        timer_touch_long_press_consumed_ = false;
    }

    // Pass-through to the base class so registered widgets still see events.
    Screen::handleTouch(x, y, pressed);
}

bool MainScreen::isInTimerHitbox(int16_t x, int16_t y) const {
    // Loose bounding box around the big MM:SS digits drawn by drawTimer().
    int16_t y_top = STATUS_BAR_HEIGHT + MODE_LABEL_HEIGHT + SEQUENCE_HEIGHT;
    int16_t y_bot = y_top + TIMER_HEIGHT + TIMER_GAP;
    return x >= 40 && x <= SCREEN_WIDTH - 40 && y >= y_top && y <= y_bot;
}

void MainScreen::performCycleReset() {
    // Stop any running/paused timer and reset the breadcrumb back to dot 1.
    // resetDailyCounter() also zeros completed_today so the cycle truly starts
    // over (otherwise the badge would still show e.g. 3 from the prior pass).
    state_machine_.reset();
    sequence_.reset();
    sequence_.resetDailyCounter();
    Serial.println("[MainScreen] Cycle reset: session=1, completed_today=0");
}

void MainScreen::drawLongPressProgress(Renderer& renderer) {
    // Show a thin growing bar at the top while either long-press source is held.
    // Picks whichever start is non-zero so the bar reflects whatever the user
    // is actively holding (the other source is guaranteed 0 in practice).
    uint32_t start = btn_a_press_start_ != 0 ? btn_a_press_start_
                                             : timer_touch_press_start_;
    if (start == 0) return;

    uint32_t held = millis() - start;
    if (held < 200) return;  // Tiny grace period so accidental presses don't flash

    uint32_t clamped = held > LONG_PRESS_MS ? LONG_PRESS_MS : held;
    int16_t fill_w = (int16_t)((SCREEN_WIDTH * clamped) / LONG_PRESS_MS);
    int16_t y = STATUS_BAR_HEIGHT;  // Just below the status bar
    renderer.drawRect(0, y, SCREEN_WIDTH, 3, Renderer::Color(TFT_DARKGREY), true);
    renderer.drawRect(0, y, fill_w, 3, Renderer::Color(TFT_ORANGE), true);
}

void MainScreen::drawResetDialog(Renderer& renderer) {
    int16_t dx = (SCREEN_WIDTH - DIALOG_W) / 2;
    int16_t dy = (SCREEN_HEIGHT - DIALOG_H) / 2;

    // Dim the screen behind the dialog so it reads as modal.
    renderer.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Renderer::Color(TFT_BLACK), true);

    // Dialog box
    renderer.drawRect(dx, dy, DIALOG_W, DIALOG_H, Renderer::Color(TFT_NAVY), true);
    renderer.drawRect(dx, dy, DIALOG_W, DIALOG_H, Renderer::Color(TFT_WHITE), false);

    renderer.setTextDatum(TC_DATUM);
    renderer.drawString(SCREEN_WIDTH / 2, dy + 12, "Reset cycle?",
                        &fonts::Font4, Renderer::Color(TFT_WHITE));
    renderer.drawString(SCREEN_WIDTH / 2, dy + 42, "Breadcrumb back to dot 1",
                        &fonts::Font2, Renderer::Color(TFT_LIGHTGRAY));

    // Buttons
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

void MainScreen::draw(Renderer& renderer) {
    if (!needs_redraw_) return;

    // Clear background
    renderer.clear(Renderer::Color(TFT_BLACK));

    // Draw status bar at top
    status_bar_.draw(renderer);

    // Draw mode label and session info
    drawModeLabel(renderer);

    // Draw sequence indicator dots
    sequence_indicator_.draw(renderer);

    // Draw large timer display
    drawTimer(renderer);

    // Draw progress bar only when timer is active
    auto state = state_machine_.getState();
    if (state == TimerStateMachine::State::ACTIVE ||
        state == TimerStateMachine::State::PAUSED) {
        progress_bar_.draw(renderer);
    }

    // Draw task name
    drawTaskName(renderer);

    // Long-press progress bar (over the top, only while a hold is in progress)
    drawLongPressProgress(renderer);

    // Reset confirmation dialog goes on top of everything else
    if (reset_dialog_visible_) {
        drawResetDialog(renderer);
    }

    // Hardware buttons drawn by ScreenManager (HardwareButtonBar)

    needs_redraw_ = false;
}

// Note: handleTouch() inherited from Screen base class (delegates to TouchEventManager)

void MainScreen::drawModeLabel(Renderer& renderer) {
    // Draw "Session X/Y" text (work sessions only)
    char label[32];
    auto session = sequence_.getCurrentSession();

    // Use new methods to track work sessions (not intervals)
    uint8_t current_work_session = sequence_.getCurrentWorkSession();
    uint8_t total_work_sessions = sequence_.getTotalWorkSessions();

    snprintf(label, sizeof(label), "Session %d/%d",
             current_work_session, total_work_sessions);

    int16_t y = STATUS_BAR_HEIGHT + 5;
    renderer.setTextDatum(TC_DATUM);  // Top-center
    renderer.drawString(SCREEN_WIDTH / 2, y, label,
                       &fonts::Font2, Renderer::Color(TFT_CYAN));

    // Today's completed Pomodoros — sourced from Statistics (NVS-backed) so
    // the counter auto-resets at midnight via Statistics::ensureTodayExists().
    // PomodoroSequence's completed_today is still used for the breadcrumb dots.
    char count_str[8];
    uint16_t today_count = statistics_.getToday().completed_sessions;
    snprintf(count_str, sizeof(count_str), "%u", today_count);
    renderer.setTextDatum(TR_DATUM);  // Top-right
    renderer.drawString(SCREEN_WIDTH - 10, y, count_str,
                       &fonts::Font4, Renderer::Color(TFT_GREEN));
}

void MainScreen::drawTimer(Renderer& renderer) {
    // Get time to display
    uint8_t minutes, seconds;
    auto state = state_machine_.getState();

    if (state == TimerStateMachine::State::IDLE) {
        // When idle, show the upcoming session duration instead of 00:00
        auto session = sequence_.getCurrentSession();
        minutes = session.duration_min;
        seconds = 0;
    } else {
        // When active or paused, show remaining time
        state_machine_.getRemainingTime(minutes, seconds);
    }

    // Format as MM:SS
    char time_str[6];
    snprintf(time_str, sizeof(time_str), "%02d:%02d", minutes, seconds);

    // Calculate position
    int16_t y = STATUS_BAR_HEIGHT + MODE_LABEL_HEIGHT + SEQUENCE_HEIGHT + TIMER_GAP + TIMER_FONT.height/2;

    // Draw time in large font
    renderer.setTextDatum(MC_DATUM);  // Middle-center
    Renderer::Color time_color = Renderer::Color(TFT_WHITE);

    // Color based on state
    if (state == TimerStateMachine::State::ACTIVE) {
        if (sequence_.isWorkSession()) {
            time_color = Renderer::Color(TFT_RED);
        } else {
            time_color = Renderer::Color(TFT_GREEN);
        }
    } else if (state == TimerStateMachine::State::PAUSED) {
        time_color = Renderer::Color(TFT_YELLOW);
    }

    renderer.drawString(SCREEN_WIDTH / 2, y, time_str,
                       &TIMER_FONT, time_color);
}

void MainScreen::drawTaskName(Renderer& renderer) {
    // Calculate position based on whether progress bar is visible
    // If progress bar visible: place below it with spacing
    // If no progress bar: center in remaining space below timer
    auto state = state_machine_.getState();
    bool progress_visible = (state == TimerStateMachine::State::ACTIVE ||
                            state == TimerStateMachine::State::PAUSED);

    int16_t controls_bottom = STATUS_BAR_HEIGHT + MODE_LABEL_HEIGHT
                              + SEQUENCE_HEIGHT + TIMER_HEIGHT + TIMER_GAP;
    if (progress_visible) {
        controls_bottom += PROGRESS_HEIGHT + TIMER_GAP;
    }
    int16_t available_space = SCREEN_HEIGHT - controls_bottom - STATUS_BAR_HEIGHT;

    auto cur_session = sequence_.getCurrentSession();
    bool is_break =
        (cur_session.type == PomodoroSequence::SessionType::SHORT_BREAK ||
         cur_session.type == PomodoroSequence::SessionType::LONG_BREAK);

    renderer.setTextDatum(MC_DATUM);

    if (is_break) {
        // Two-line layout during a break: project name on top, "REST" tag
        // below in cyan so the user can see at a glance that the timer is
        // counting *down* a break, not a focus session. Available space is
        // roughly 40px when the progress bar is up, comfortably enough for
        // two Font2 lines (~16px each).
        int16_t y_top = controls_bottom + available_space / 3;
        int16_t y_bot = controls_bottom + (2 * available_space) / 3;
        renderer.drawString(SCREEN_WIDTH / 2, y_top, task_name_,
                           &fonts::Font2, Renderer::Color(TFT_LIGHTGRAY));
        renderer.drawString(SCREEN_WIDTH / 2, y_bot, "REST",
                           &fonts::Font2, Renderer::Color(TFT_CYAN));
    } else {
        int16_t y = controls_bottom + (available_space / 2);
        renderer.drawString(SCREEN_WIDTH / 2, y, task_name_,
                           &fonts::Font2, Renderer::Color(TFT_LIGHTGRAY));
    }
}

void MainScreen::updateButtons() {
    // Button update logic removed - now using hardware buttons with dynamic labels
    // Button labels are updated by ScreenManager via getButtonLabels()
}

// Hardware button interface implementation
void MainScreen::getButtonLabels(const char*& btnA, const char*& btnB, const char*& btnC) {
    // Reset confirm dialog takes over the labels while open so the user can see
    // which hardware button does what (matches the on-screen Yes/No buttons).
    if (reset_dialog_visible_) {
        btnA = "No";       // Cancel reset
        btnB = "";
        btnC = "Yes";      // Confirm reset
        return;
    }

    // BtnA label depends on timer state (Start/Pause/Resume)
    auto state = state_machine_.getState();
    if (state == TimerStateMachine::State::IDLE) {
        btnA = "Start";
    } else if (state == TimerStateMachine::State::ACTIVE) {
        btnA = "Pause";
    } else if (state == TimerStateMachine::State::PAUSED) {
        btnA = "Resume";
    } else {
        btnA = "";  // Unknown state
    }

    // BtnB and BtnC are always Stats and Settings
    btnB = "Stats";
    btnC = "Set";
}

void MainScreen::onButtonA() {
    // If the reset confirmation is open, BtnA cancels it (no action otherwise).
    if (reset_dialog_visible_) {
        Serial.println("[MainScreen] BtnA: Cancel reset dialog");
        reset_dialog_visible_ = false;
        needs_redraw_ = true;
        return;
    }

    auto state = state_machine_.getState();

    if (state == TimerStateMachine::State::IDLE) {
        // Defer Start until release: update() distinguishes a short press
        // (fires Start) from a long press (opens the reset dialog) by timing
        // wasReleased(). Tracking begins here on wasPressed().
        btn_a_press_start_ = millis();
        btn_a_long_press_consumed_ = false;
        return;
    } else if (state == TimerStateMachine::State::ACTIVE) {
        Serial.println("[MainScreen] BtnA: Pause timer");
        state_machine_.handleEvent(TimerStateMachine::Event::PAUSE);
    } else if (state == TimerStateMachine::State::PAUSED) {
        Serial.println("[MainScreen] BtnA: Resume timer");
        state_machine_.handleEvent(TimerStateMachine::Event::RESUME);
    }

    needs_redraw_ = true;
}

void MainScreen::onButtonB() {
    if (reset_dialog_visible_) {
        // Dialog uses BtnA/BtnC for Cancel/Confirm; BtnB is inert while open.
        return;
    }
    // Navigate to Stats screen
    Serial.println("[MainScreen] BtnB: Navigate to Stats");
    if (navigate_callback_) {
        navigate_callback_(ScreenID::STATS);
    }
}

void MainScreen::onButtonC() {
    if (reset_dialog_visible_) {
        Serial.println("[MainScreen] BtnC: Confirm reset");
        performCycleReset();
        reset_dialog_visible_ = false;
        needs_redraw_ = true;
        return;
    }
    // Navigate to Settings screen
    Serial.println("[MainScreen] BtnC: Navigate to Settings");
    if (navigate_callback_) {
        navigate_callback_(ScreenID::SETTINGS);
    }
}
