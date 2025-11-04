/*
 * M5 Pomodoro Timer v2 - Core Components Test
 *
 * Testing PomodoroSequence and TimerStateMachine implementations
 */

#include <M5Unified.h>
#include "core/PomodoroSequence.h"
#include "core/TimerStateMachine.h"

PomodoroSequence sequence;
TimerStateMachine* stateMachine = nullptr;

uint32_t lastUpdate = 0;

void onStateChange(TimerStateMachine::State oldState, TimerStateMachine::State newState) {
    Serial.printf("[CALLBACK] State changed: %d -> %d\n",
                  static_cast<int>(oldState), static_cast<int>(newState));
}

void onTimeout() {
    Serial.println("[CALLBACK] Timer timeout!");
}

void setup() {
    // Initialize Serial first for debugging
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n=================================");
    Serial.println("M5 Pomodoro Timer v2 - " FIRMWARE_VERSION);
    Serial.println("Core Components Test");
    Serial.println("=================================");

    // Initialize M5Unified
    auto cfg = M5.config();
    cfg.clear_display = true;
    cfg.internal_imu = false;  // Not needed for this test
    cfg.internal_rtc = true;
    cfg.internal_spk = false;
    cfg.internal_mic = false;
    M5.begin(cfg);

    Serial.println("[OK] M5Unified initialized");

    // Initialize PomodoroSequence
    sequence.setMode(PomodoroSequence::Mode::CLASSIC);
    sequence.start();

    Serial.printf("[Sequence] Mode: CLASSIC, Session: %u/%u\n",
                  sequence.getCurrentSessionNumber(), sequence.getTotalSessions());

    auto session = sequence.getCurrentSession();
    Serial.printf("[Sequence] Current: %s, %u minutes\n",
                  session.type == PomodoroSequence::SessionType::WORK ? "WORK" : "BREAK",
                  session.duration_min);

    // Initialize StateMachine
    stateMachine = new TimerStateMachine(sequence);
    stateMachine->onStateChange(onStateChange);
    stateMachine->onTimeout(onTimeout);

    Serial.printf("[StateMachine] Initial state: %s\n", stateMachine->getStateName());

    // Display on screen
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_GREEN);
    M5.Display.setTextDatum(MC_DATUM);
    M5.Display.setFont(&fonts::Font4);
    M5.Display.drawString("M5 Pomodoro v2", 160, 60);

    M5.Display.setTextColor(TFT_CYAN);
    M5.Display.setFont(&fonts::Font2);
    M5.Display.drawString("Core Test Mode", 160, 100);

    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.drawString("Touch to START", 160, 140);
    M5.Display.drawString("Check Serial Monitor", 160, 180);

    Serial.println("\n[OK] Initialization complete!");
    Serial.println("Touch screen to start timer (30 seconds for testing)\n");

    lastUpdate = millis();
}

void loop() {
    M5.update();

    // Calculate delta time
    uint32_t now = millis();
    uint32_t delta = now - lastUpdate;
    lastUpdate = now;

    // Update state machine
    if (stateMachine) {
        stateMachine->update(delta);
    }

    // Touch to start timer
    auto touch = M5.Touch.getDetail();
    if (touch.wasPressed()) {
        if (stateMachine->getState() == TimerStateMachine::State::IDLE) {
            Serial.println("[USER] Starting timer...");
            stateMachine->handleEvent(TimerStateMachine::Event::START);
        } else if (stateMachine->getState() == TimerStateMachine::State::RUNNING) {
            Serial.println("[USER] Pausing timer...");
            stateMachine->handleEvent(TimerStateMachine::Event::PAUSE);
        } else if (stateMachine->getState() == TimerStateMachine::State::PAUSED) {
            Serial.println("[USER] Resuming timer...");
            stateMachine->handleEvent(TimerStateMachine::Event::RESUME);
        } else if (stateMachine->getState() == TimerStateMachine::State::COMPLETED) {
            Serial.println("[USER] Session complete! Starting next...");
            stateMachine->handleEvent(TimerStateMachine::Event::START);
        }
    }

    // Display timer status
    if (stateMachine && stateMachine->isActive()) {
        static uint32_t lastDisplay = 0;
        if (now - lastDisplay >= 1000) {  // Update display every second
            lastDisplay = now;

            uint8_t minutes, seconds;
            stateMachine->getRemainingTime(minutes, seconds);

            M5.Display.fillRect(0, 100, 320, 60, TFT_BLACK);
            M5.Display.setTextColor(TFT_YELLOW);
            M5.Display.setFont(&fonts::Font6);

            char timeStr[16];
            snprintf(timeStr, sizeof(timeStr), "%02u:%02u", minutes, seconds);
            M5.Display.drawString(timeStr, 160, 130);

            M5.Display.setFont(&fonts::Font2);
            M5.Display.setTextColor(TFT_CYAN);
            M5.Display.drawString(stateMachine->getStateName(), 160, 170);

            Serial.printf("[Timer] %s: %02u:%02u (%u%%)\n",
                          stateMachine->getStateName(),
                          minutes, seconds,
                          stateMachine->getProgressPercent());
        }
    }

    delay(10);
}
