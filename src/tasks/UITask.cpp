#include "UITask.h"
#include <M5Unified.h>
#include "../ui/Renderer.h"
#include "../ui/ScreenManager.h"
#include "../hardware/AudioPlayer.h"

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
extern AudioPlayer* g_audioPlayer;
extern TimerStateMachine* g_stateMachine;
extern PomodoroSequence* g_sequence;

// Task timing
static uint32_t g_lastUpdate = 0;
static uint32_t g_lastSecond = 0;
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

        // Small delay to prevent watchdog and allow other tasks to run
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
