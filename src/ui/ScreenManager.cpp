#include "ScreenManager.h"
#include <M5Unified.h>

// Global navigation callback (set by ScreenManager, called by screen button handlers)
void (*g_navigate_callback)(ScreenID) = nullptr;

// Static instance for global callback
static ScreenManager* g_screen_manager_instance = nullptr;

ScreenManager::ScreenManager(TimerStateMachine& state_machine,
                             PomodoroSequence& sequence,
                             Statistics& statistics,
                             Config& config,
                             LEDController& led_controller)
    : main_screen_(state_machine, sequence),
      stats_screen_(statistics),
      settings_screen_(config),
      pause_screen_(state_machine, led_controller),
      current_screen_(ScreenID::MAIN),
      state_machine_(state_machine),
      last_state_(TimerStateMachine::State::IDLE) {

    // Set global instance for navigation callback
    g_screen_manager_instance = this;

    // Set global navigation callback (called by screen button handlers)
    g_navigate_callback = [](ScreenID screen) {
        if (g_screen_manager_instance) {
            g_screen_manager_instance->navigate(screen);
        }
    };

    Serial.println("[ScreenManager] Initialized with 4 screens");
    Serial.println("[ScreenManager] Global navigation callback set");
}

void ScreenManager::navigate(ScreenID screen) {
    // Prevent navigation to PauseScreen (auto-managed by state machine)
    if (screen == ScreenID::PAUSE && current_screen_ != ScreenID::PAUSE) {
        Serial.println("[ScreenManager] Warning: Cannot manually navigate to PauseScreen (auto-managed)");
        return;
    }

    if (screen == current_screen_) {
        Serial.println("[ScreenManager] Already on requested screen");
        return;
    }

    const char* screen_names[] = {"MAIN", "STATS", "SETTINGS", "PAUSE"};
    Serial.printf("[ScreenManager] Navigating: %s -> %s\n",
                  screen_names[(int)current_screen_],
                  screen_names[(int)screen]);

    current_screen_ = screen;

    // Mark new screen dirty for immediate redraw
    switch (current_screen_) {
        case ScreenID::MAIN:
            main_screen_.markDirty();
            break;
        case ScreenID::STATS:
            stats_screen_.markDirty();
            break;
        case ScreenID::SETTINGS:
            settings_screen_.markDirty();
            break;
        case ScreenID::PAUSE:
            pause_screen_.markDirty();
            break;
    }
}

void ScreenManager::checkAutoNavigation() {
    TimerStateMachine::State current_state = state_machine_.getState();

    // Auto-switch to PauseScreen when timer is paused
    if (current_state == TimerStateMachine::State::PAUSED && current_screen_ != ScreenID::PAUSE) {
        Serial.println("[ScreenManager] Auto-navigation: PAUSED state -> PauseScreen");
        current_screen_ = ScreenID::PAUSE;
        pause_screen_.markDirty();
    }

    // Auto-return to MainScreen when timer resumes or stops
    if (last_state_ == TimerStateMachine::State::PAUSED &&
        current_state != TimerStateMachine::State::PAUSED &&
        current_screen_ == ScreenID::PAUSE) {
        Serial.println("[ScreenManager] Auto-navigation: Resumed/Stopped -> MainScreen");
        current_screen_ = ScreenID::MAIN;
        main_screen_.markDirty();
    }

    last_state_ = current_state;
}

void ScreenManager::update(uint32_t deltaMs) {
    // Check for state-driven auto-navigation (PAUSED <-> MainScreen)
    checkAutoNavigation();

    // Update active screen
    switch (current_screen_) {
        case ScreenID::MAIN:
            main_screen_.update(deltaMs);
            break;
        case ScreenID::STATS:
            stats_screen_.update(deltaMs);
            break;
        case ScreenID::SETTINGS:
            settings_screen_.update(deltaMs);
            break;
        case ScreenID::PAUSE:
            pause_screen_.update(deltaMs);
            break;
    }
}

void ScreenManager::draw(Renderer& renderer) {
    // Draw active screen
    switch (current_screen_) {
        case ScreenID::MAIN:
            main_screen_.draw(renderer);
            break;
        case ScreenID::STATS:
            stats_screen_.draw(renderer);
            break;
        case ScreenID::SETTINGS:
            settings_screen_.draw(renderer);
            break;
        case ScreenID::PAUSE:
            pause_screen_.draw(renderer);
            break;
    }
}

void ScreenManager::handleTouch(int16_t x, int16_t y, bool pressed) {
    // Forward touch events to active screen
    switch (current_screen_) {
        case ScreenID::MAIN:
            main_screen_.handleTouch(x, y, pressed);
            break;
        case ScreenID::STATS:
            stats_screen_.handleTouch(x, y, pressed);
            break;
        case ScreenID::SETTINGS:
            settings_screen_.handleTouch(x, y, pressed);
            break;
        case ScreenID::PAUSE:
            pause_screen_.handleTouch(x, y, pressed);
            break;
    }
}

void ScreenManager::updateStatus(uint8_t battery, bool charging, bool wifi,
                                 const char* mode, uint8_t hour, uint8_t minute) {
    // Propagate status updates to ALL screens (not just active)
    // This ensures status bar stays fresh when navigating between screens
    main_screen_.updateStatus(battery, charging, wifi, mode, hour, minute);
    stats_screen_.updateStatus(battery, charging, wifi, mode, hour, minute);
    settings_screen_.updateStatus(battery, charging, wifi, mode, hour, minute);
    pause_screen_.updateStatus(battery, charging, wifi, mode, hour, minute);
}
