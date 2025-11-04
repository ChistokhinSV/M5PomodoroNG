/*
 * M5 Pomodoro Timer v2 - Pause Screen Test
 *
 * Testing PauseScreen with timer state machine in PAUSED state
 */

#include <M5Unified.h>
#include "ui/Renderer.h"
#include "ui/screens/PauseScreen.h"
#include "core/TimerStateMachine.h"
#include "core/PomodoroSequence.h"
#include "hardware/LEDController.h"

Renderer renderer;
uint32_t lastUpdate = 0;
uint32_t lastSecond = 0;

// Core components
PomodoroSequence sequence;
TimerStateMachine stateMachine(sequence);
LEDController ledController;

// Pause screen
PauseScreen pauseScreen(stateMachine, ledController);

void setup() {
    // Initialize M5
    auto cfg = M5.config();
    M5.begin(cfg);

    Serial.begin(115200);
    delay(100);

    Serial.println("\n=================================");
    Serial.println("M5 Pomodoro v2 - Pause Screen Test");
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

    // Initialize LED controller
    if (!ledController.begin()) {
        Serial.println("[ERROR] Failed to initialize LED controller");
    } else {
        Serial.println("[OK] LED controller initialized");
    }

    // Initialize sequence and state machine
    sequence.setMode(PomodoroSequence::Mode::CLASSIC);
    sequence.start();
    Serial.println("[OK] Pomodoro sequence initialized (Classic mode)");

    // Start a timer and immediately pause it
    Serial.println("[Test] Starting timer...");
    stateMachine.handleEvent(TimerStateMachine::Event::START);

    delay(100);  // Let it run briefly

    Serial.println("[Test] Pausing timer...");
    stateMachine.handleEvent(TimerStateMachine::Event::PAUSE);

    // Verify we're in PAUSED state
    auto state = stateMachine.getState();
    if (state == TimerStateMachine::State::PAUSED) {
        Serial.println("[OK] Timer is PAUSED");

        uint8_t minutes, seconds;
        stateMachine.getRemainingTime(minutes, seconds);
        Serial.printf("[Test] Frozen time: %02d:%02d\n", minutes, seconds);
    } else {
        Serial.println("[ERROR] Timer not in PAUSED state!");
    }

    // Update pause screen status
    uint8_t battery = M5.Power.getBatteryLevel();
    bool charging = M5.Power.isCharging();
    pauseScreen.updateStatus(battery, charging, false, "PAU", 10, 45);

    Serial.println("\nControls:");
    Serial.println("- Touch 'Resume' button (left) to resume timer");
    Serial.println("- Touch 'Stop' button (right) to stop and reset");
    Serial.println("\n[OK] Pause screen initialized\n");

    lastUpdate = millis();
    lastSecond = millis();
}

void loop() {
    M5.update();

    // Handle touch events
    auto touch = M5.Touch.getDetail();

    if (touch.wasPressed()) {
        pauseScreen.handleTouch(touch.x, touch.y, true);
    }

    if (touch.wasReleased()) {
        pauseScreen.handleTouch(touch.x, touch.y, false);
    }

    // Update at 30 FPS (~33ms per frame)
    uint32_t now = millis();
    uint32_t deltaMs = now - lastUpdate;

    if (deltaMs >= 33) {
        lastUpdate = now;

        // Update state machine (maintains paused state)
        stateMachine.update(deltaMs);

        // Update LED controller (no deltaMs parameter)
        ledController.update();

        // Update pause screen
        pauseScreen.update(deltaMs);

        // Draw
        pauseScreen.draw(renderer);

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

        // Update mode based on state
        const char* mode = "PAU";
        auto state = stateMachine.getState();
        if (state == TimerStateMachine::State::RUNNING) {
            mode = "RUN";
        } else if (state == TimerStateMachine::State::IDLE) {
            mode = "IDL";
        }

        pauseScreen.updateStatus(battery, charging, false, mode, hour, minute);

        // Log state changes
        static TimerStateMachine::State last_state = TimerStateMachine::State::PAUSED;
        if (state != last_state) {
            Serial.printf("[State Change] %d -> %d\n", (int)last_state, (int)state);
            last_state = state;
        }
    }

    delay(1);  // Prevent watchdog
}
