#ifndef MAINSCREEN_H
#define MAINSCREEN_H

#include "../Renderer.h"
#include "../widgets/StatusBar.h"
#include "../widgets/ProgressBar.h"
#include "../widgets/SequenceIndicator.h"
#include "../widgets/Button.h"
#include "../../core/TimerStateMachine.h"
#include "../../core/PomodoroSequence.h"

/**
 * Main timer screen - primary UI for Pomodoro timer
 *
 * Layout (320×240):
 * ┌─────────────────────────────────┐
 * │ [WiFi][Mode][Time][Battery]     │ ← StatusBar (20px)
 * ├─────────────────────────────────┤
 * │      Session 2/4  [Classic]     │ ← Mode label (20px)
 * │           ● ● ○ ○                │ ← SequenceIndicator (20px)
 * ├─────────────────────────────────┤
 * │           24:35                  │ ← Timer display (80px, large font)
 * │        ▓▓▓▓▓▓▓▓░░░░              │ ← ProgressBar (20px)
 * ├─────────────────────────────────┤
 * │    [Update NetBox docs]          │ ← Task name (30px, scrolling)
 * ├─────────────────────────────────┤
 * │ [Start]  [Pause]  [Stats]       │ ← Action buttons (40px)
 * └─────────────────────────────────┘
 *
 * Features:
 * - Real-time countdown timer
 * - Visual progress indication
 * - Session tracking with dots
 * - State-based button visibility
 * - Today's completion count badge
 */
class MainScreen {
public:
    MainScreen(TimerStateMachine& state_machine, PomodoroSequence& sequence);

    // Lifecycle
    void draw(Renderer& renderer);
    void update(uint32_t deltaMs);
    void handleTouch(int16_t x, int16_t y, bool pressed);

    // Configuration
    void setTaskName(const char* task);
    void updateStatus(uint8_t battery, bool charging, bool wifi, const char* mode, uint8_t hour, uint8_t minute);
    void markDirty() { needs_redraw_ = true; }

private:
    TimerStateMachine& state_machine_;
    PomodoroSequence& sequence_;

    // Widgets
    StatusBar status_bar_;
    ProgressBar progress_bar_;
    SequenceIndicator sequence_indicator_;
    Button btn_start_;
    Button btn_pause_;
    Button btn_stop_;
    Button btn_stats_;
    Button btn_settings_;

    // State
    char task_name_[64];
    uint32_t last_update_ms_;
    bool needs_redraw_;

    // Layout constants
    static constexpr int16_t SCREEN_WIDTH = 320;
    static constexpr int16_t SCREEN_HEIGHT = 240;
    static constexpr int16_t STATUS_BAR_HEIGHT = 20;
    static constexpr int16_t MODE_LABEL_HEIGHT = 20;
    static constexpr int16_t SEQUENCE_HEIGHT = 20;
    static constexpr int16_t TIMER_HEIGHT = 80;
    static constexpr int16_t PROGRESS_HEIGHT = 20;
    static constexpr int16_t TASK_HEIGHT = 30;
    static constexpr int16_t BUTTON_HEIGHT = 40;

    // Drawing helpers
    void drawModeLabel(Renderer& renderer);
    void drawTimer(Renderer& renderer);
    void drawTaskName(Renderer& renderer);
    void updateButtons();

    // Button callbacks
    static void onStartPress();
    static void onPausePress();
    static void onStopPress();
    static void onStatsPress();
    static void onSettingsPress();

    // Static instance pointer for callbacks
    static MainScreen* instance_;
};

#endif // MAINSCREEN_H
