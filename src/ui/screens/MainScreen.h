#ifndef MAINSCREEN_H
#define MAINSCREEN_H

#include <functional>
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
class MainScreen {
public:
    MainScreen(TimerStateMachine& state_machine,
               PomodoroSequence& sequence,
               NavigationCallback navigate_callback);

    // Lifecycle
    void draw(Renderer& renderer);
    void update(uint32_t deltaMs);
    void handleTouch(int16_t x, int16_t y, bool pressed);

    // Configuration
    void setTaskName(const char* task);
    void updateStatus(uint8_t battery, bool charging, bool wifi, const char* mode, uint8_t hour, uint8_t minute);
    void markDirty() { needs_redraw_ = true; }

    // Hardware button interface
    void getButtonLabels(const char*& btnA, const char*& btnB, const char*& btnC);
    void onButtonA();  // Start/Pause
    void onButtonB();  // Stats
    void onButtonC();  // Settings

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
    bool needs_redraw_;
    int16_t TIMER_HEIGHT;

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
};

#endif // MAINSCREEN_H
