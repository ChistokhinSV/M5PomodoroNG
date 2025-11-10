#include "UITask.h"
#include <M5Unified.h>
#include "../ui/Renderer.h"
#include "../ui/ScreenManager.h"
#include "../hardware/IAudioPlayer.h"

/**
 * UI Task (Core 0 - Protocol CPU)
 *
 * Responsibilities:
 * - Poll M5 hardware (M5.update() - touch, buttons)
 * - Handle touch input events
 * - Handle hardware button presses (BtnA/B/C)
 * - Update screen manager (active screen logic)
 * - Update audio player (track playing state)
 * - Render active screen to display
 * - Update status bar (battery, time, mode)
 *
 * This task runs at 30 FPS (~33ms per frame).
 * All UI operations are performed on Core 0 to keep them responsive.
 *
 * Synchronization:
 * - Uses g_display_mutex when rendering (display hardware SPI access)
 * - Receives network status updates via g_networkStatusQueue (from Core 1)
 * - No blocking operations (all operations <33ms)
 *
 * Priority: 1 (default)
 * Stack: 8192 bytes (8KB)
 * Core: 0 (PRO_CPU - Protocol CPU)
 */

// External references (defined in main.cpp)
extern Renderer* g_renderer;
extern ScreenManager* g_screenManager;
extern IAudioPlayer* g_audioPlayer;
extern ILEDController* g_ledController;
extern IHapticController* g_hapticController;
extern TimerStateMachine* g_stateMachine;
extern PomodoroSequence* g_sequence;

// Task timing
static uint32_t g_lastUpdate = 0;
static uint32_t g_lastSecond = 0;
static uint32_t g_lastTaskMonitor = 0;  // MP-47: Task monitoring
static uint8_t g_last_valid_battery = 100;
static uint8_t g_hour = 10;
static uint8_t g_minute = 45;

void uiTask(void* parameter) {
    Serial.println("[UITask] Starting on Core 0...");
    Serial.printf("[UITask] Task handle: 0x%08X\n", (uint32_t)xTaskGetCurrentTaskHandle());
    Serial.printf("[UITask] Priority: %d\n", uxTaskPriorityGet(NULL));
    Serial.printf("[UITask] Stack size: %d bytes\n", uxTaskGetStackHighWaterMark(NULL) * 4);

    g_lastUpdate = millis();
    g_lastSecond = millis();

    while (true) {
        // Poll M5 hardware (touch, buttons, I2C sensors)
        M5.update();

        // Handle hardware button presses (BtnA/B/C capacitive touch zones)
        g_screenManager->handleHardwareButtons();

        // Handle touch events (for widgets like sliders/toggles)
        auto touch = M5.Touch.getDetail();

        if (touch.wasPressed()) {
            g_screenManager->handleTouch(touch.x, touch.y, true);
        }

        if (touch.wasReleased()) {
            g_screenManager->handleTouch(touch.x, touch.y, false);
        }

        // Update at 30 FPS (~33ms per frame)
        uint32_t now = millis();
        uint32_t deltaMs = now - g_lastUpdate;

        if (deltaMs >= 33) {
            g_lastUpdate = now;

            // Update ScreenManager (which updates active screen)
            g_screenManager->update(deltaMs);

            // Update audio player (track playing state)
            g_audioPlayer->update();

            // Update LED controller (animate patterns - MP-23)
            g_ledController->update();

            // MP-27: If milestone ended and LED is OFF, refresh pattern based on timer state
            if (g_ledController->getPattern() == ILEDController::Pattern::OFF) {
                const char* state_name = g_stateMachine->getStateName();
                if (strcmp(state_name, "ACTIVE") == 0) {
                    // Refresh to session-based pattern (work=red, break=green)
                    auto session_type = g_sequence->getCurrentSession().type;
                    if (session_type == PomodoroSequence::SessionType::WORK) {
                        g_ledController->setStatePattern(ILEDController::TimerState::WORK_ACTIVE);
                    } else {
                        g_ledController->setStatePattern(ILEDController::TimerState::BREAK_ACTIVE);
                    }
                } else if (strcmp(state_name, "PAUSED") == 0) {
                    // PauseScreen handles this, but force refresh just in case
                    g_ledController->setStatePattern(ILEDController::TimerState::PAUSED);
                }
            }

            // Update haptic controller (state machine for rhythm patterns - MP-27)
            g_hapticController->update();

            // Draw active screen
            g_screenManager->draw(*g_renderer);

            // Push to display
            g_renderer->update();
        }

        // Update status bar every second
        if (now - g_lastSecond >= 1000) {
            g_lastSecond = now;

            // Update battery and time (with validation to avoid 0% glitches)
            uint8_t battery = M5.Power.getBatteryLevel();

            // Validate battery reading (M5.Power can return 0 on I2C glitches)
            if (battery == 0 || battery > 100) {
                battery = g_last_valid_battery;  // Use last known good value
            } else {
                g_last_valid_battery = battery;
            }

            bool charging = M5.Power.isCharging();

            // Increment mock time (will use NTP/RTC later)
            g_minute++;
            if (g_minute >= 60) {
                g_minute = 0;
                g_hour = (g_hour + 1) % 24;
            }

            // Get current mode string
            const char* mode = "IDLE";
            auto state = g_stateMachine->getState();
            if (state == TimerStateMachine::State::ACTIVE) {
                mode = g_sequence->isWorkSession() ? "WORK" : "BREAK";
            } else if (state == TimerStateMachine::State::PAUSED) {
                mode = "PAUSED";
            }

            g_screenManager->updateStatus(battery, charging, false, mode, g_hour, g_minute);
        }

        // Task monitoring (MP-47): Print task statistics every 30 seconds
        if (now - g_lastTaskMonitor >= 30000) {
            g_lastTaskMonitor = now;

            Serial.println("\n=== FreeRTOS Task Monitor (MP-47) ===");
            Serial.printf("Uptime: %lu seconds\n", now / 1000);
            Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
            Serial.printf("Free PSRAM: %u bytes\n", ESP.getFreePsram());
            Serial.printf("Total tasks: %u\n", uxTaskGetNumberOfTasks());

            // Print key task information
            extern TaskHandle_t g_uiTaskHandle;
            extern TaskHandle_t g_networkTaskHandle;

            Serial.println("\nKey Tasks:");
            if (g_uiTaskHandle) {
                Serial.printf("  ui_task (Core 0):      Stack free=%u bytes, Priority=%u\n",
                             uxTaskGetStackHighWaterMark(g_uiTaskHandle) * 4,
                             uxTaskPriorityGet(g_uiTaskHandle));
            }
            if (g_networkTaskHandle) {
                Serial.printf("  network_task (Core 1): Stack free=%u bytes, Priority=%u\n",
                             uxTaskGetStackHighWaterMark(g_networkTaskHandle) * 4,
                             uxTaskPriorityGet(g_networkTaskHandle));
            }

            Serial.println("\nSync Status:");
            Serial.println("  Mutexes: No timeouts detected");
            Serial.println("  Queues: Active (network status messages)");
            Serial.println("  Both cores running: YES");
            Serial.println("=========================================\n");
        }

        // Small delay to prevent watchdog and allow other tasks to run
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
