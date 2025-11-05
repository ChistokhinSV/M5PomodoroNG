#include "MainScreen.h"
#include "../ScreenManager.h"
#include <M5Unified.h>
#include <stdio.h>
#include <string.h>

// Static instance pointer for button callbacks
MainScreen* MainScreen::instance_ = nullptr;

MainScreen::MainScreen(TimerStateMachine& state_machine, PomodoroSequence& sequence)
    : state_machine_(state_machine),
      sequence_(sequence),
      last_update_ms_(0),
      needs_redraw_(true) {

    // Set static instance for callbacks
    instance_ = this;

    strcpy(task_name_, "Focus Session");

    // Configure widgets with layout positions
    // Status bar at top (320×20)
    status_bar_.setBounds(0, 0, SCREEN_WIDTH, STATUS_BAR_HEIGHT);

    // Sequence indicator below status bar (200×20, centered)
    sequence_indicator_.setBounds(60, STATUS_BAR_HEIGHT + 10, 200, SEQUENCE_HEIGHT);
    sequence_indicator_.setDotsPerGroup(4);  // Classic mode default

    // Progress bar in middle (280×20, centered)
    int16_t progress_y = STATUS_BAR_HEIGHT + MODE_LABEL_HEIGHT + SEQUENCE_HEIGHT + TIMER_HEIGHT;
    progress_bar_.setBounds(20, progress_y, 280, PROGRESS_HEIGHT);
    progress_bar_.setShowPercentage(false);  // Don't show percentage on timer progress
    progress_bar_.setColor(Renderer::Color(TFT_RED));

    // Buttons at bottom (5 buttons, compact layout)
    int16_t button_y = SCREEN_HEIGHT - BUTTON_HEIGHT - 10;
    btn_start_.setBounds(10, button_y, 60, BUTTON_HEIGHT);
    btn_start_.setLabel("Start");
    btn_start_.setCallback(onStartPress);

    btn_pause_.setBounds(75, button_y, 60, BUTTON_HEIGHT);
    btn_pause_.setLabel("Pause");
    btn_pause_.setCallback(onPausePress);
    btn_pause_.setVisible(false);  // Hidden until timer starts

    btn_stop_.setBounds(140, button_y, 50, BUTTON_HEIGHT);
    btn_stop_.setLabel("Stop");
    btn_stop_.setCallback(onStopPress);

    btn_stats_.setBounds(195, button_y, 55, BUTTON_HEIGHT);
    btn_stats_.setLabel("Stats");
    btn_stats_.setCallback(onStatsPress);

    btn_settings_.setBounds(255, button_y, 55, BUTTON_HEIGHT);
    btn_settings_.setLabel("Set");
    btn_settings_.setCallback(onSettingsPress);
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
    // Update state machine
    state_machine_.update(deltaMs);

    // Update progress bar (inverse: 100% at start, 0% at end)
    uint8_t progress = 100 - state_machine_.getProgressPercent();
    progress_bar_.setProgress(progress);

    // Update sequence indicator
    auto session = sequence_.getCurrentSession();
    sequence_indicator_.setSession(session.number - 1,  // Convert 1-based to 0-based
                                   sequence_.getCompletedToday());

    // Update button visibility based on state
    updateButtons();

    // Update widget animations
    progress_bar_.update(deltaMs);
    sequence_indicator_.update(deltaMs);

    needs_redraw_ = true;
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

    // Draw progress bar
    progress_bar_.draw(renderer);

    // Draw task name
    drawTaskName(renderer);

    // Draw buttons
    btn_start_.draw(renderer);
    btn_pause_.draw(renderer);
    btn_stop_.draw(renderer);
    btn_stats_.draw(renderer);
    btn_settings_.draw(renderer);

    needs_redraw_ = false;
}

void MainScreen::handleTouch(int16_t x, int16_t y, bool pressed) {
    if (pressed) {
        // Touch down - check button hits
        if (btn_start_.hitTest(x, y)) {
            btn_start_.onTouch(x, y);
        } else if (btn_pause_.hitTest(x, y)) {
            btn_pause_.onTouch(x, y);
        } else if (btn_stop_.hitTest(x, y)) {
            btn_stop_.onTouch(x, y);
        } else if (btn_stats_.hitTest(x, y)) {
            btn_stats_.onTouch(x, y);
        } else if (btn_settings_.hitTest(x, y)) {
            btn_settings_.onTouch(x, y);
        }
    } else {
        // Touch up - release all buttons
        btn_start_.onRelease(x, y);
        btn_pause_.onRelease(x, y);
        btn_stop_.onRelease(x, y);
        btn_stats_.onRelease(x, y);
        btn_settings_.onRelease(x, y);
    }
}

void MainScreen::drawModeLabel(Renderer& renderer) {
    // Draw "Session X/Y [Mode]" text
    char label[32];
    auto session = sequence_.getCurrentSession();
    uint8_t total_sessions = sequence_.getTotalSessions();

    const char* mode_name = "Classic";
    switch (sequence_.getMode()) {
        case PomodoroSequence::Mode::CLASSIC:
            mode_name = "Classic";
            break;
        case PomodoroSequence::Mode::STUDY:
            mode_name = "Study";
            break;
        case PomodoroSequence::Mode::CUSTOM:
            mode_name = "Custom";
            break;
    }

    snprintf(label, sizeof(label), "Session %d/%d [%s]",
             session.number, total_sessions, mode_name);

    int16_t y = STATUS_BAR_HEIGHT + 5;
    renderer.setTextDatum(TC_DATUM);  // Top-center
    renderer.drawString(SCREEN_WIDTH / 2, y, label,
                       &fonts::Font2, Renderer::Color(TFT_CYAN));

    // Draw today's completion count badge
    char count_str[8];
    snprintf(count_str, sizeof(count_str), "%d", sequence_.getCompletedToday());
    renderer.setTextDatum(TR_DATUM);  // Top-right
    renderer.drawString(SCREEN_WIDTH - 10, y, count_str,
                       &fonts::Font4, Renderer::Color(TFT_GREEN));
}

void MainScreen::drawTimer(Renderer& renderer) {
    // Get remaining time
    uint8_t minutes, seconds;
    state_machine_.getRemainingTime(minutes, seconds);

    // Format as MM:SS
    char time_str[6];
    snprintf(time_str, sizeof(time_str), "%02d:%02d", minutes, seconds);

    // Calculate position (centered, below sequence indicator)
    int16_t y = STATUS_BAR_HEIGHT + MODE_LABEL_HEIGHT + SEQUENCE_HEIGHT + 40;

    // Draw time in large font
    renderer.setTextDatum(MC_DATUM);  // Middle-center
    Renderer::Color time_color = Renderer::Color(TFT_WHITE);

    // Color based on state
    auto state = state_machine_.getState();
    if (state == TimerStateMachine::State::RUNNING) {
        if (sequence_.isWorkSession()) {
            time_color = Renderer::Color(TFT_RED);
        } else {
            time_color = Renderer::Color(TFT_GREEN);
        }
    } else if (state == TimerStateMachine::State::PAUSED) {
        time_color = Renderer::Color(TFT_YELLOW);
    }

    renderer.drawString(SCREEN_WIDTH / 2, y, time_str,
                       &fonts::Font8, time_color);
}

void MainScreen::drawTaskName(Renderer& renderer) {
    // Draw task name (truncated if too long)
    int16_t y = STATUS_BAR_HEIGHT + MODE_LABEL_HEIGHT + SEQUENCE_HEIGHT +
                TIMER_HEIGHT + PROGRESS_HEIGHT + 15;

    renderer.setTextDatum(TC_DATUM);  // Top-center
    renderer.drawString(SCREEN_WIDTH / 2, y, task_name_,
                       &fonts::Font2, Renderer::Color(TFT_LIGHTGRAY));
}

void MainScreen::updateButtons() {
    auto state = state_machine_.getState();

    switch (state) {
        case TimerStateMachine::State::IDLE:
            btn_start_.setVisible(true);
            btn_start_.setLabel("Start");
            btn_pause_.setVisible(false);
            btn_stop_.setVisible(false);
            break;

        case TimerStateMachine::State::RUNNING:
        case TimerStateMachine::State::BREAK:
            btn_start_.setVisible(false);
            btn_pause_.setVisible(true);
            btn_pause_.setLabel("Pause");
            btn_stop_.setVisible(true);
            break;

        case TimerStateMachine::State::PAUSED:
            btn_start_.setVisible(false);
            btn_pause_.setVisible(true);
            btn_pause_.setLabel("Resume");
            btn_stop_.setVisible(true);
            break;

        case TimerStateMachine::State::COMPLETED:
            btn_start_.setVisible(true);
            btn_start_.setLabel("Next");
            btn_pause_.setVisible(false);
            btn_stop_.setVisible(false);
            break;
    }
}

// Button callback implementations
void MainScreen::onStartPress() {
    if (!instance_) return;

    auto state = instance_->state_machine_.getState();

    if (state == TimerStateMachine::State::IDLE) {
        // Start new session
        instance_->state_machine_.handleEvent(TimerStateMachine::Event::START);
        Serial.println("[MainScreen] Start timer");
    } else if (state == TimerStateMachine::State::COMPLETED) {
        // Move to next session
        instance_->sequence_.advance();
        instance_->state_machine_.handleEvent(TimerStateMachine::Event::START);
        Serial.println("[MainScreen] Next session");
    }

    instance_->needs_redraw_ = true;
}

void MainScreen::onPausePress() {
    if (!instance_) return;

    auto state = instance_->state_machine_.getState();

    if (state == TimerStateMachine::State::RUNNING ||
        state == TimerStateMachine::State::BREAK) {
        // Pause timer
        instance_->state_machine_.handleEvent(TimerStateMachine::Event::PAUSE);
        Serial.println("[MainScreen] Pause timer");
    } else if (state == TimerStateMachine::State::PAUSED) {
        // Resume timer
        instance_->state_machine_.handleEvent(TimerStateMachine::Event::RESUME);
        Serial.println("[MainScreen] Resume timer");
    }

    instance_->needs_redraw_ = true;
}

void MainScreen::onStopPress() {
    if (!instance_) return;

    // Stop timer and return to IDLE
    instance_->state_machine_.handleEvent(TimerStateMachine::Event::STOP);
    Serial.println("[MainScreen] Stop timer");

    instance_->needs_redraw_ = true;
}

void MainScreen::onStatsPress() {
    if (!instance_) return;

    // Navigate to statistics screen
    if (g_navigate_callback) {
        g_navigate_callback(ScreenID::STATS);
    }
}

void MainScreen::onSettingsPress() {
    if (!instance_) return;

    // Navigate to settings screen
    if (g_navigate_callback) {
        g_navigate_callback(ScreenID::SETTINGS);
    }
}
