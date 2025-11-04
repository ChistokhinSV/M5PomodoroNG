/*
 * M5 Pomodoro Timer v2 - Main Screen Test
 *
 * Testing MainScreen with TimerStateMachine and PomodoroSequence
 */

#include <M5Unified.h>
#include "ui/Renderer.h"
#include "ui/screens/MainScreen.h"
#include "core/PomodoroSequence.h"
#include "core/TimerStateMachine.h"

Renderer renderer;
uint32_t lastUpdate = 0;
uint32_t lastSecond = 0;

// Core components
PomodoroSequence sequence;
TimerStateMachine stateMachine(sequence);

// Main screen
MainScreen mainScreen(stateMachine, sequence);

void setup() {
    // Initialize M5
    auto cfg = M5.config();
    M5.begin(cfg);

    Serial.begin(115200);
    delay(100);

    Serial.println("\n=================================");
    Serial.println("M5 Pomodoro v2 - Main Screen Test");
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

    // Initialize sequence (Classic mode, 25-5-25-5-25-5-25-15)
    sequence.setMode(PomodoroSequence::Mode::CLASSIC);
    sequence.start();
    Serial.println("[OK] Pomodoro sequence initialized (Classic mode)");

    // Update main screen status
    uint8_t battery = M5.Power.getBatteryLevel();
    bool charging = M5.Power.isCharging();
    mainScreen.updateStatus(battery, charging, false, "BAL", 10, 45);
    mainScreen.setTaskName("Deep Work Session");

    Serial.println("\nControls:");
    Serial.println("- Touch 'Start' to begin timer");
    Serial.println("- Touch 'Pause' to pause/resume");
    Serial.println("- Touch 'Stop' to reset");
    Serial.println("- Touch 'Stats' (placeholder)");
    Serial.println("\n[OK] Main screen initialized\n");

    lastUpdate = millis();
    lastSecond = millis();
}

void loop() {
    M5.update();

    // Handle touch events
    auto touch = M5.Touch.getDetail();

    if (touch.wasPressed()) {
        mainScreen.handleTouch(touch.x, touch.y, true);
    }

    if (touch.wasReleased()) {
        mainScreen.handleTouch(touch.x, touch.y, false);
    }

    // Update at 30 FPS (~33ms per frame)
    uint32_t now = millis();
    uint32_t deltaMs = now - lastUpdate;

    if (deltaMs >= 33) {
        lastUpdate = now;

        // Update main screen (includes state machine update)
        mainScreen.update(deltaMs);

        // Draw
        mainScreen.draw(renderer);

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

        mainScreen.updateStatus(battery, charging, false, "BAL", hour, minute);
    }

    delay(1);  // Prevent watchdog
}
