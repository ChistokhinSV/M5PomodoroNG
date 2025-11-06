#include "MainScreen.h"
#include "../ScreenManager.h"
#include <M5Unified.h>
#include <stdio.h>
#include <string.h>

MainScreen::MainScreen(TimerStateMachine& state_machine,
                       PomodoroSequence& sequence,
                       NavigationCallback navigate_callback)
    : state_machine_(state_machine),
      sequence_(sequence),
      navigate_callback_(navigate_callback),
      last_update_ms_(0),
      needs_redraw_(true) {

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
    // Update state machine
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
    sequence_indicator_.setTotalSessions(sequence_.getTotalSessions() * 4);
    sequence_indicator_.setDotsPerGroup(4);  // 4 work intervals per session

    // Determine current work session index (0-based)
    // During work: working on next work session after completed ones
    // During break: doesn't matter, we pulse the last completed work session
    uint8_t current_work_session = sequence_.getCompletedToday();
    bool in_break = !sequence_.isWorkSession();

    sequence_indicator_.setSession(current_work_session,
                                   sequence_.getCompletedToday(),
                                   in_break);

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

    // Draw progress bar only when timer is active
    auto state = state_machine_.getState();
    if (state == TimerStateMachine::State::ACTIVE ||
        state == TimerStateMachine::State::PAUSED) {
        progress_bar_.draw(renderer);
    }

    // Draw task name
    drawTaskName(renderer);

    // Hardware buttons drawn by ScreenManager (HardwareButtonBar)

    needs_redraw_ = false;
}

void MainScreen::handleTouch(int16_t x, int16_t y, bool pressed) {
    // Touch handling removed - now uses hardware buttons
    // Hardware button handling done by ScreenManager via onButtonA/B/C()
}

void MainScreen::drawModeLabel(Renderer& renderer) {
    // Draw "Session X/Y [Mode]" text
    char label[32];
    auto session = sequence_.getCurrentSession();
    uint8_t total_sessions = sequence_.getTotalSessions();

    // Calculate current work session number (not total session including breaks)
    uint8_t current_work_session;
    if (sequence_.isWorkSession()) {
        // Working on next work session
        current_work_session = sequence_.getCompletedToday() + 1;
    } else {
        // Resting after completing a work session
        current_work_session = sequence_.getCompletedToday();
    }

    const char* mode_name = "Classic";
    PomodoroSequence::Mode current_mode = sequence_.getMode();
    Serial.printf("[MainScreen] drawModeLabel: getMode() returns %d (0=CLASSIC, 1=STUDY, 2=CUSTOM)\n", (int)current_mode);

    switch (current_mode) {
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
             current_work_session, total_sessions, mode_name);

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

    int16_t y;
    int16_t controls_bottom;
    int16_t available_space;

    // timer bottom
    controls_bottom = STATUS_BAR_HEIGHT + MODE_LABEL_HEIGHT + SEQUENCE_HEIGHT + TIMER_HEIGHT + TIMER_GAP;

    // progress bar bottom if visible
    if (progress_visible) {
        controls_bottom += PROGRESS_HEIGHT + TIMER_GAP;
    }

    // Calculate available space
    available_space = SCREEN_HEIGHT - controls_bottom - STATUS_BAR_HEIGHT;

    y = controls_bottom + (available_space / 2);

    renderer.setTextDatum(MC_DATUM);  // Middle-center for better vertical centering
    renderer.drawString(SCREEN_WIDTH / 2, y, task_name_,
                       &fonts::Font2, Renderer::Color(TFT_LIGHTGRAY));
}

void MainScreen::updateButtons() {
    // Button update logic removed - now using hardware buttons with dynamic labels
    // Button labels are updated by ScreenManager via getButtonLabels()
}

// Hardware button interface implementation
void MainScreen::getButtonLabels(const char*& btnA, const char*& btnB, const char*& btnC) {
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
    // Start/Pause/Resume based on state
    auto state = state_machine_.getState();

    if (state == TimerStateMachine::State::IDLE) {
        Serial.println("[MainScreen] BtnA: Start timer");
        state_machine_.handleEvent(TimerStateMachine::Event::START);
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
    // Navigate to Stats screen
    Serial.println("[MainScreen] BtnB: Navigate to Stats");
    if (navigate_callback_) {
        navigate_callback_(ScreenID::STATS);
    }
}

void MainScreen::onButtonC() {
    // Navigate to Settings screen
    Serial.println("[MainScreen] BtnC: Navigate to Settings");
    if (navigate_callback_) {
        navigate_callback_(ScreenID::SETTINGS);
    }
}
