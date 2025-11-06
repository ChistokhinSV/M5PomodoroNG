#ifndef PAUSESCREEN_H
#define PAUSESCREEN_H

#include <functional>
#include "../Renderer.h"
#include "../widgets/StatusBar.h"
#include "../widgets/Button.h"
#include "../../core/TimerStateMachine.h"
#include "../../hardware/ILEDController.h"

// Forward declare ScreenID from ScreenManager.h (avoid circular include)
enum class ScreenID;
using NavigationCallback = std::function<void(ScreenID)>;

/**
 * Pause screen - displays paused timer state
 *
 * Layout (320×240):
 * ┌─────────────────────────────────┐
 * │ [WiFi][Mode][Time][Battery]     │ ← StatusBar (20px)
 * ├─────────────────────────────────┤
 * │                                 │
 * │           PAUSED                │ ← Large text (50px height)
 * │                                 │
 * ├─────────────────────────────────┤
 * │           24:35                 │ ← Frozen timer (60px)
 * │                                 │
 * ├─────────────────────────────────┤
 * │                                 │
 * │  [Resume]         [Stop]        │ ← Buttons (40px)
 * │                                 │
 * └─────────────────────────────────┘
 *
 * Features:
 * - Amber color scheme (paused state indicator)
 * - Frozen timer display (remaining time from state machine)
 * - Resume button → return to RUNNING/BREAK state
 * - Stop button → return to IDLE state
 * - Pulsing amber LED effect
 */
class PauseScreen {
public:
    PauseScreen(TimerStateMachine& state_machine,
                ILEDController& led_controller,
                NavigationCallback navigate_callback);

    // Lifecycle
    void draw(Renderer& renderer);
    void update(uint32_t deltaMs);
    void handleTouch(int16_t x, int16_t y, bool pressed);

    // Configuration
    void updateStatus(uint8_t battery, bool charging, bool wifi, const char* mode, uint8_t hour, uint8_t minute);
    void markDirty() { needs_redraw_ = true; }

    // Hardware button interface
    void getButtonLabels(const char*& btnA, const char*& btnB, const char*& btnC);
    void onButtonA();  // Resume timer
    void onButtonB();  // (unused)
    void onButtonC();  // Stop timer

private:
    TimerStateMachine& state_machine_;
    ILEDController& led_controller_;
    NavigationCallback navigate_callback_;

    // Widgets
    StatusBar status_bar_;
    // Note: Button widgets removed, now using hardware buttons

    // State
    bool needs_redraw_;

    // Layout constants
    static constexpr int16_t SCREEN_WIDTH = 320;
    static constexpr int16_t SCREEN_HEIGHT = 240;
    static constexpr int16_t STATUS_BAR_HEIGHT = 20;
    static constexpr int16_t PAUSED_TEXT_Y = 70;
    static constexpr int16_t TIMER_Y = 120;
    static constexpr int16_t BUTTON_Y = 160;
    static constexpr int16_t BUTTON_WIDTH = 110;
    static constexpr int16_t BUTTON_HEIGHT = 40;

    // Amber color scheme (RGB565)
    static constexpr uint16_t COLOR_AMBER = 0xFDA0;  // Orange (255, 165, 0)
    static constexpr uint16_t COLOR_AMBER_DARK = 0x7D40;  // Dark amber

    // Drawing helpers
    void drawPausedText(Renderer& renderer);
    void drawTimer(Renderer& renderer);
};

#endif // PAUSESCREEN_H
