#include "ScreenManager.h"
#include <M5Unified.h>

ScreenManager::ScreenManager(TimerStateMachine& state_machine,
                             PomodoroSequence& sequence,
                             Statistics& statistics,
                             Config& config,
                             LEDController& led_controller)
    : main_screen_(state_machine, sequence,
                   [this](ScreenID screen) { this->navigate(screen); }),
      stats_screen_(statistics,
                    [this](ScreenID screen) { this->navigate(screen); }),
      settings_screen_(config,
                       [this](ScreenID screen) { this->navigate(screen); }),
      pause_screen_(state_machine, led_controller,
                    [this](ScreenID screen) { this->navigate(screen); }),
      current_screen_(ScreenID::MAIN),
      state_machine_(state_machine),
      last_state_(TimerStateMachine::State::IDLE) {

    // Configure hardware button bar
    button_bar_.setBounds(0, 218, 320, 22);
    updateButtonLabels();  // Set initial labels for MainScreen

    Serial.println("[ScreenManager] Initialized with 4 screens");
    Serial.println("[ScreenManager] Navigation callbacks set via lambdas");
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

    // Update button labels for new screen
    updateButtonLabels();
}

void ScreenManager::checkAutoNavigation() {
    TimerStateMachine::State current_state = state_machine_.getState();

    // Auto-switch to PauseScreen when timer is paused
    if (current_state == TimerStateMachine::State::PAUSED && current_screen_ != ScreenID::PAUSE) {
        Serial.println("[ScreenManager] Auto-navigation: PAUSED state -> PauseScreen");
        current_screen_ = ScreenID::PAUSE;
        pause_screen_.markDirty();
        updateButtonLabels();  // Update button labels for PauseScreen
    }

    // Auto-return to MainScreen when timer resumes or stops
    if (last_state_ == TimerStateMachine::State::PAUSED &&
        current_state != TimerStateMachine::State::PAUSED &&
        current_screen_ == ScreenID::PAUSE) {
        Serial.println("[ScreenManager] Auto-navigation: Resumed/Stopped -> MainScreen");
        current_screen_ = ScreenID::MAIN;
        main_screen_.markDirty();
        updateButtonLabels();  // Update button labels for MainScreen
    }

    // Update button labels on any state change (for MainScreen button state-dependent labels)
    if (last_state_ != current_state && current_screen_ == ScreenID::MAIN) {
        updateButtonLabels();  // Refresh button labels when state changes (ACTIVE/IDLE/PAUSED)
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

    // Draw hardware button bar on top of screen content
    button_bar_.draw(renderer);
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

void ScreenManager::handleHardwareButtons() {
    // Handle hardware button presses (BtnA, BtnB, BtnC)
    // Note: M5.update() must be called before this method

    if (M5.BtnA.wasPressed()) {
        Serial.println("[ScreenManager] BtnA pressed");
        switch (current_screen_) {
            case ScreenID::MAIN:
                main_screen_.onButtonA();
                break;
            case ScreenID::STATS:
                stats_screen_.onButtonA();
                break;
            case ScreenID::SETTINGS:
                settings_screen_.onButtonA();
                break;
            case ScreenID::PAUSE:
                pause_screen_.onButtonA();
                break;
        }
        updateButtonLabels();  // Refresh button labels after action
    }

    if (M5.BtnB.wasPressed()) {
        Serial.println("[ScreenManager] BtnB pressed");
        switch (current_screen_) {
            case ScreenID::MAIN:
                main_screen_.onButtonB();
                break;
            case ScreenID::STATS:
                stats_screen_.onButtonB();
                break;
            case ScreenID::SETTINGS:
                settings_screen_.onButtonB();
                break;
            case ScreenID::PAUSE:
                pause_screen_.onButtonB();
                break;
        }
        updateButtonLabels();  // Refresh button labels after action
    }

    if (M5.BtnC.wasPressed()) {
        Serial.println("[ScreenManager] BtnC pressed");
        switch (current_screen_) {
            case ScreenID::MAIN:
                main_screen_.onButtonC();
                break;
            case ScreenID::STATS:
                stats_screen_.onButtonC();
                break;
            case ScreenID::SETTINGS:
                settings_screen_.onButtonC();
                break;
            case ScreenID::PAUSE:
                pause_screen_.onButtonC();
                break;
        }
        updateButtonLabels();  // Refresh button labels after action
    }
}

void ScreenManager::updateButtonLabels() {
    // Update button labels based on active screen
    const char* labelA = "";
    const char* labelB = "";
    const char* labelC = "";
    bool enabledA = true;
    bool enabledB = true;
    bool enabledC = true;

    switch (current_screen_) {
        case ScreenID::MAIN:
            main_screen_.getButtonLabels(labelA, labelB, labelC);
            break;
        case ScreenID::STATS:
            stats_screen_.getButtonLabels(labelA, labelB, labelC);
            break;
        case ScreenID::SETTINGS:
            settings_screen_.getButtonLabels(labelA, labelB, labelC, enabledA, enabledB, enabledC);
            break;
        case ScreenID::PAUSE:
            pause_screen_.getButtonLabels(labelA, labelB, labelC);
            break;
    }

    button_bar_.setLabels(labelA, labelB, labelC);
    button_bar_.setEnabled(enabledA, enabledB, enabledC);
}
