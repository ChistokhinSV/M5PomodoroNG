#ifndef SCREEN_MANAGER_H
#define SCREEN_MANAGER_H

#include "screens/MainScreen.h"
#include "screens/StatsScreen.h"
#include "screens/SettingsScreen.h"
#include "screens/PauseScreen.h"
#include "../core/TimerStateMachine.h"
#include "../core/PomodoroSequence.h"
#include "../core/Statistics.h"
#include "../core/Config.h"
#include "../hardware/LEDController.h"
#include "Renderer.h"

/**
 * Screen identifiers for navigation
 */
enum class ScreenID {
    MAIN,       // Primary timer display (work/break sessions)
    STATS,      // Weekly statistics chart
    SETTINGS,   // Configuration UI (5 pages)
    PAUSE       // Paused timer state (auto-managed)
};

/**
 * Global navigation callback (set by ScreenManager, called by screen button handlers)
 *
 * Usage in screen callbacks:
 *   if (g_navigate_callback) {
 *       g_navigate_callback(ScreenID::STATS);
 *   }
 */
extern void (*g_navigate_callback)(ScreenID);

/**
 * ScreenManager - Handles navigation between screens
 *
 * Features:
 * - Manages lifecycle of all 4 screens (MainScreen, StatsScreen, SettingsScreen, PauseScreen)
 * - Global navigation callback for screen button handlers
 * - Auto-navigation: PAUSED state → PauseScreen, resume → MainScreen
 * - Duck-typed interface (no base class, all screens have same method signatures)
 * - Status bar updates propagate to all screens
 *
 * Architecture:
 * - All screens are stack-allocated members (always exist)
 * - Screens take dependencies by reference (not owned by screens)
 * - Current screen pointer tracks active screen
 * - Simple navigation (no history/back stack)
 *
 * Usage:
 *   ScreenManager manager(state_machine, sequence, statistics, config, led_controller);
 *
 *   // Main loop
 *   manager.update(deltaMs);
 *   manager.draw(renderer);
 *   manager.handleTouch(x, y, pressed);
 *
 *   // Manual navigation (or via screen button callbacks)
 *   manager.navigate(ScreenID::STATS);
 */
class ScreenManager {
public:
    ScreenManager(TimerStateMachine& state_machine,
                  PomodoroSequence& sequence,
                  Statistics& statistics,
                  Config& config,
                  LEDController& led_controller);

    // Navigation
    void navigate(ScreenID screen);
    ScreenID getCurrentScreen() const { return current_screen_; }

    // Lifecycle methods (called from main loop)
    void update(uint32_t deltaMs);
    void draw(Renderer& renderer);
    void handleTouch(int16_t x, int16_t y, bool pressed);

    // Status bar updates (propagate to all screens)
    void updateStatus(uint8_t battery, bool charging, bool wifi,
                     const char* mode, uint8_t hour, uint8_t minute);

private:
    // Screen instances (stack-allocated, always exist)
    MainScreen main_screen_;
    StatsScreen stats_screen_;
    SettingsScreen settings_screen_;
    PauseScreen pause_screen_;

    // Active screen tracking
    ScreenID current_screen_;

    // Auto-navigation state (monitor state machine for PAUSED)
    TimerStateMachine& state_machine_;
    TimerStateMachine::State last_state_;

    // Auto-navigation logic
    void checkAutoNavigation();
};

#endif // SCREEN_MANAGER_H
