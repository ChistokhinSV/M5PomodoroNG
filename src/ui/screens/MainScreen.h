#ifndef MAINSCREEN_H
#define MAINSCREEN_H

#include <functional>
#include "../Screen.h"
#include "../Renderer.h"
#include "../widgets/StatusBar.h"
#include "../widgets/ProgressBar.h"
#include "../widgets/SequenceIndicator.h"
#include "../widgets/Button.h"
#include "../../core/TimerStateMachine.h"
#include "../../core/PomodoroSequence.h"

#include <M5Unified.h>

// Forward declare ScreenID from ScreenManager.h (avoid circular include)
enum class ScreenID;
using NavigationCallback = std::function<void(ScreenID)>;

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
class MainScreen : public Screen {
public:
    MainScreen(TimerStateMachine& state_machine,
               PomodoroSequence& sequence,
               NavigationCallback navigate_callback);

    // Override Screen interface
    void draw(Renderer& renderer) override;
    void update(uint32_t deltaMs) override;
    void updateStatus(uint8_t battery, bool charging, bool wifi, const char* mode, uint8_t hour, uint8_t minute) override;
    void getButtonLabels(const char*& btnA, const char*& btnB, const char*& btnC) override;
    void onButtonA() override;  // Start/Pause (defers Start in IDLE — see update())
    void onButtonB() override;  // Stats
    void onButtonC() override;  // Settings
    void handleTouch(int16_t x, int16_t y, bool pressed) override;

    // Screen-specific methods
    void setTaskName(const char* task);

private:
    TimerStateMachine& state_machine_;
    PomodoroSequence& sequence_;
    NavigationCallback navigate_callback_;

    // Widgets
    StatusBar status_bar_;
    ProgressBar progress_bar_;
    SequenceIndicator sequence_indicator_;
    // Note: Button widgets removed, now using hardware buttons

    // State
    char task_name_[64];
    uint32_t last_update_ms_;
    int16_t TIMER_HEIGHT;
    // Note: needs_redraw_ inherited from Screen base class

    // Long-press → cycle-reset state. Two trigger paths:
    //   - BtnA held while in IDLE (don't fire Start until release; if held long
    //     enough, show the confirm dialog instead).
    //   - Touch held inside the timer digits area (any state).
    // Both draw a progress arc during the hold and pop the same confirm dialog.
    uint32_t btn_a_press_start_ = 0;     // 0 = not held; else millis() at press
    bool btn_a_long_press_consumed_ = false;  // True once the hold tripped the dialog (skip Start on release)
    uint32_t timer_touch_press_start_ = 0;
    bool timer_touch_long_press_consumed_ = false;
    bool reset_dialog_visible_ = false;
    static constexpr uint32_t LONG_PRESS_MS = 1500;

    #define SMALL_FONT fonts::Font2
    #define TIMER_FONT fonts::Font8

    // Layout constants
    static constexpr int16_t SCREEN_WIDTH = 320;
    static constexpr int16_t SCREEN_HEIGHT = 240;
    static constexpr int16_t STATUS_BAR_HEIGHT = 20;
    static constexpr int16_t MODE_LABEL_HEIGHT = 20;
    static constexpr int16_t SEQUENCE_HEIGHT = 20;
    // Space between elements
    static constexpr int16_t TIMER_GAP = 10;
    static constexpr int16_t TASK_LABEL_GAP = 8;
    static constexpr int16_t PROGRESS_HEIGHT = 20;
    static constexpr int16_t TASK_HEIGHT = 30;
    static constexpr int16_t BUTTON_HEIGHT = 40;

    // Drawing helpers
    void drawModeLabel(Renderer& renderer);
    void drawTimer(Renderer& renderer);
    void drawTaskName(Renderer& renderer);
    void updateButtons();

    // Long-press helpers
    bool isInTimerHitbox(int16_t x, int16_t y) const;
    void drawLongPressProgress(Renderer& renderer);
    void drawResetDialog(Renderer& renderer);
    void performCycleReset();

    // Reset-dialog hit zones (Yes/No buttons). Recomputed in drawResetDialog().
    static constexpr int16_t DIALOG_W = 240;
    static constexpr int16_t DIALOG_H = 110;
    static constexpr int16_t DIALOG_BTN_W = 90;
    static constexpr int16_t DIALOG_BTN_H = 32;
};

#endif // MAINSCREEN_H
