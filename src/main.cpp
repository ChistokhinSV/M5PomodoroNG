/*
 * M5 Pomodoro Timer v2 - ScreenManager Test
 *
 * Testing navigation between MainScreen, StatsScreen, SettingsScreen, and PauseScreen
 */

#include <M5Unified.h>
#include "ui/Renderer.h"
#include "ui/ScreenManager.h"
#include "core/Config.h"
#include "core/TimerStateMachine.h"
#include "core/PomodoroSequence.h"
#include "core/Statistics.h"
#include "hardware/LEDController.h"

Renderer renderer;
uint32_t lastUpdate = 0;
uint32_t lastSecond = 0;

// Core components
Config config;
PomodoroSequence sequence;
TimerStateMachine stateMachine(sequence);  // Needs sequence reference
Statistics statistics;
LEDController ledController;

// Screen Manager (manages all screens and navigation)
ScreenManager* screenManager = nullptr;

void setup() {
    // Initialize M5
    auto cfg = M5.config();
    M5.begin(cfg);

    Serial.begin(115200);
    delay(100);

    Serial.println("\n=================================");
    Serial.println("M5 Pomodoro v2 - ScreenManager Test");
    Serial.println("=================================");

    // Check PSRAM
    if (psramFound()) {
        size_t psram_size = ESP.getPsramSize();
        size_t free_psram = ESP.getFreePsram();
        Serial.printf("[OK] PSRAM detected: %d bytes total, %d bytes free\n",
                     psram_size, free_psram);
    } else {
        Serial.println("[WARN] No PSRAM detected");
    }

    // Initialize renderer
    if (!renderer.begin()) {
        Serial.println("[ERROR] Failed to initialize renderer");
        while (1) delay(100);
    }
    Serial.println("[OK] Renderer initialized");

    // Initialize config
    if (!config.begin()) {
        Serial.println("[ERROR] Failed to initialize config");
    } else {
        Serial.println("[OK] Config initialized");
    }

    // Initialize statistics
    if (!statistics.begin()) {
        Serial.println("[ERROR] Failed to initialize statistics");
    } else {
        Serial.println("[OK] Statistics initialized");
    }

    // Initialize LED controller
    if (!ledController.begin()) {
        Serial.println("[ERROR] Failed to initialize LED controller");
    } else {
        Serial.println("[OK] LED controller initialized");
    }

    // Initialize state machine and sequence
    auto pomodoro_config = config.getPomodoro();
    sequence.setMode(PomodoroSequence::Mode::CLASSIC);
    sequence.setSessionsBeforeLong(pomodoro_config.sessions_before_long);
    Serial.println("[OK] State machine and sequence initialized");

    // Create ScreenManager (owns all 4 screens)
    screenManager = new ScreenManager(stateMachine, sequence, statistics, config, ledController);
    Serial.println("[OK] ScreenManager initialized with 4 screens");

    // Update initial status
    uint8_t battery = M5.Power.getBatteryLevel();
    bool charging = M5.Power.isCharging();
    screenManager->updateStatus(battery, charging, false, "IDLE", 10, 45);

    Serial.println("\nControls:");
    Serial.println("- MainScreen: [Start] [Stats] [Set] buttons");
    Serial.println("- StatsScreen: [<- Back] button");
    Serial.println("- SettingsScreen: [<- Back] [Prev] [Next] buttons (5 pages)");
    Serial.println("- PauseScreen: Auto-navigate when timer paused");
    Serial.println("\nNavigation flow:");
    Serial.println("  MainScreen <-> StatsScreen (Stats/Back buttons)");
    Serial.println("  MainScreen <-> SettingsScreen (Set/Back buttons)");
    Serial.println("  MainScreen <-> PauseScreen (auto-managed by state machine)");
    Serial.println("\n[OK] All screens ready\n");

    lastUpdate = millis();
    lastSecond = millis();
}

void loop() {
    M5.update();

    // Handle touch events
    auto touch = M5.Touch.getDetail();

    if (touch.wasPressed()) {
        screenManager->handleTouch(touch.x, touch.y, true);
    }

    if (touch.wasReleased()) {
        screenManager->handleTouch(touch.x, touch.y, false);
    }

    // Update at 30 FPS (~33ms per frame)
    uint32_t now = millis();
    uint32_t deltaMs = now - lastUpdate;

    if (deltaMs >= 33) {
        lastUpdate = now;

        // Update ScreenManager (which updates active screen)
        screenManager->update(deltaMs);

        // Draw active screen
        screenManager->draw(renderer);

        // Push to display
        renderer.update();
    }

    // Update status bar every second
    if (now - lastSecond >= 1000) {
        lastSecond = now;

        // Update battery and time
        uint8_t battery = M5.Power.getBatteryLevel();
        bool charging = M5.Power.isCharging();

        // Get current time (use millis for now, will use RTC later)
        static uint8_t hour = 10;
        static uint8_t minute = 45;
        minute++;
        if (minute >= 60) {
            minute = 0;
            hour = (hour + 1) % 24;
        }

        // Get current mode string
        const char* mode = "IDLE";
        auto state = stateMachine.getState();
        if (state == TimerStateMachine::State::RUNNING) {
            mode = sequence.isWorkSession() ? "WORK" : "BREAK";
        } else if (state == TimerStateMachine::State::PAUSED) {
            mode = "PAUSED";
        } else if (state == TimerStateMachine::State::COMPLETED) {
            mode = "DONE";
        }

        screenManager->updateStatus(battery, charging, false, mode, hour, minute);
    }

    delay(1);  // Prevent watchdog
}
