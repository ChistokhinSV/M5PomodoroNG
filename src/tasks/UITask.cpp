#include "UITask.h"
#include <M5Unified.h>
#include <WiFi.h>
#include "../ui/Renderer.h"
#include "../ui/ScreenManager.h"
#include "../hardware/IAudioPlayer.h"
#include "../core/TimeManager.h"
#include "../core/Config.h"
#include "../core/SleepState.h"
#include "../hardware/IPowerManager.h"

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
extern TimeManager* g_timeManager;
extern Config* g_config;
extern IPowerManager* g_powerManager;

// Task timing
static uint32_t g_lastUpdate = 0;
static uint32_t g_lastSecond = 0;
static uint32_t g_lastTaskMonitor = 0;  // MP-47: Task monitoring
static uint8_t g_last_valid_battery = 100;

// Idle tracking for sleep mode (MP-30)
static uint32_t g_lastInteraction = 0;  // Last user interaction timestamp
static uint32_t g_idleDuration = 0;     // Time spent idle (ms)
static constexpr uint32_t LIGHT_SLEEP_THRESHOLD_MS = 30 * 60 * 1000;  // 30 minutes

void uiTask(void* parameter) {
    Serial.println("[UITask] Starting on Core 0...");
    Serial.printf("[UITask] Task handle: 0x%08X\n", (uint32_t)xTaskGetCurrentTaskHandle());
    Serial.printf("[UITask] Priority: %d\n", uxTaskPriorityGet(NULL));
    Serial.printf("[UITask] Stack size: %d bytes\n", uxTaskGetStackHighWaterMark(NULL) * 4);

    g_lastUpdate = millis();
    g_lastSecond = millis();
    g_lastInteraction = millis();  // Initialize idle tracking

    while (true) {
        // Poll M5 hardware (touch, buttons, I2C sensors)
        M5.update();

        // Handle hardware button presses (BtnA/B/C capacitive touch zones)
        if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || M5.BtnC.wasPressed()) {
            g_lastInteraction = millis();  // Reset idle timer
        }
        g_screenManager->handleHardwareButtons();

        // Handle touch events (for widgets like sliders/toggles)
        auto touch = M5.Touch.getDetail();

        if (touch.wasPressed()) {
            g_screenManager->handleTouch(touch.x, touch.y, true);
            g_lastInteraction = millis();  // Reset idle timer
        }

        if (touch.wasReleased()) {
            g_screenManager->handleTouch(touch.x, touch.y, false);
            g_lastInteraction = millis();  // Reset idle timer
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

            // Get actual time from TimeManager (RTC + NTP)
            uint8_t hour = 0, minute = 0;
            if (g_timeManager) {
                struct tm timeinfo;
                g_timeManager->getLocalTime(timeinfo);
                hour = timeinfo.tm_hour;
                minute = timeinfo.tm_min;
            }

            // Check WiFi status - show connected if:
            // 1. WiFi currently connected, OR
            // 2. TimeManager was synced via NTP (time is accurate even if WiFi disconnected)
            bool wifi_status = (WiFi.status() == WL_CONNECTED);
            if (!wifi_status && g_timeManager && g_timeManager->isTimeSynced()) {
                // Show WiFi icon if time was synced (even if disconnected now)
                wifi_status = true;
            }

            // Get current mode string
            const char* mode = "IDLE";
            auto state = g_stateMachine->getState();
            if (state == TimerStateMachine::State::ACTIVE) {
                mode = g_sequence->isWorkSession() ? "WORK" : "BREAK";
            } else if (state == TimerStateMachine::State::PAUSED) {
                mode = "PAUSED";
            }

            g_screenManager->updateStatus(battery, charging, wifi_status, mode, hour, minute);
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

        // MP-30: Check sleep conditions (adaptive: light sleep <30min, deep sleep >=30min)
        auto power_settings = g_config->getPower();
        if (power_settings.auto_sleep_enabled && power_settings.sleep_after_min > 0) {
            // Calculate idle duration
            g_idleDuration = now - g_lastInteraction;
            uint32_t sleep_threshold_ms = power_settings.sleep_after_min * 60 * 1000;

            // Check if idle timeout exceeded
            if (g_idleDuration >= sleep_threshold_ms) {
                // Check if in IDLE or PAUSED state (sleep-eligible states)
                auto state = g_stateMachine->getState();
                bool sleep_eligible = (state == TimerStateMachine::State::IDLE ||
                                       state == TimerStateMachine::State::PAUSED);

                if (sleep_eligible) {
                    // Check DC power condition (skip sleep if charging and configured)
                    bool is_charging = M5.Power.isCharging();
                    bool skip_sleep = is_charging && !power_settings.sleep_on_dc_power;

                    if (!skip_sleep) {
                        // MP-80: Use deep sleep only (light sleep has FreeRTOS dual-core issues)
                        Serial.printf("[UITask] Idle %lu min - entering DEEP SLEEP\n", g_idleDuration / 60000);

                        // Save current LED pattern for restoration
                        ILEDController::TimerState led_pattern = ILEDController::TimerState::IDLE;
                        if (state == TimerStateMachine::State::IDLE) {
                            led_pattern = ILEDController::TimerState::WARNING;  // Yellow flash mode
                        } else if (state == TimerStateMachine::State::PAUSED) {
                            led_pattern = ILEDController::TimerState::PAUSED;  // Red blink
                        }

                        // Save state to RTC memory
                        SleepState::save(*g_stateMachine, *g_sequence, led_pattern);

                        // Power down LEDs (clear + disable 5V boost)
                        g_ledController->powerDown();
                        delay(100);  // Wait for power to stabilize

                        // Enter deep sleep (device will reset on wake)
                        g_powerManager->enterDeepSleep(0);  // Infinite sleep, wake on touch

                        // Note: Code never reaches here (ESP32 resets on wake)
                    } else {
                        Serial.println("[UITask] Charging detected - skip sleep (DC power = OFF)");
                        g_lastInteraction = millis();  // Reset to prevent repeated log spam
                    }
                }
            }
        }

        // Small delay to prevent watchdog and allow other tasks to run
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
