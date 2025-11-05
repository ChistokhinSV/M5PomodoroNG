/*
 * M5 Pomodoro Timer v2 - Settings Screen Test
 *
 * Testing SettingsScreen with 16 configurable options across 3 pages
 */

#include <M5Unified.h>
#include "ui/Renderer.h"
#include "ui/screens/SettingsScreen.h"
#include "core/Config.h"

Renderer renderer;
uint32_t lastUpdate = 0;
uint32_t lastSecond = 0;

// Core components
Config config;

// Settings screen
SettingsScreen settingsScreen(config);

void setup() {
    // Initialize M5
    auto cfg = M5.config();
    M5.begin(cfg);

    Serial.begin(115200);
    delay(100);

    Serial.println("\n=================================");
    Serial.println("M5 Pomodoro v2 - Settings Screen Test");
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

    // Print current config values
    Serial.println("\n[Config] Current settings:");
    auto pomodoro = config.getPomodoro();
    Serial.printf("  Work: %d min, Short break: %d min, Long break: %d min\n",
                 pomodoro.work_duration_min, pomodoro.short_break_min,
                 pomodoro.long_break_min);
    Serial.printf("  Sessions: %d, Auto-start breaks: %s, Auto-start work: %s\n",
                 pomodoro.sessions_before_long,
                 pomodoro.auto_start_breaks ? "ON" : "OFF",
                 pomodoro.auto_start_work ? "ON" : "OFF");

    auto ui = config.getUI();
    Serial.printf("  Brightness: %d%%, Sound: %s, Volume: %d%%\n",
                 ui.brightness, ui.sound_enabled ? "ON" : "OFF", ui.sound_volume);
    Serial.printf("  Haptic: %s, Show seconds: %s, Timeout: %ds\n",
                 ui.haptic_enabled ? "ON" : "OFF",
                 ui.show_seconds ? "ON" : "OFF",
                 ui.screen_timeout_sec);

    auto power = config.getPower();
    Serial.printf("  Auto-sleep: %s, Sleep after: %d min\n",
                 power.auto_sleep_enabled ? "ON" : "OFF",
                 power.sleep_after_min);
    Serial.printf("  Wake on rotation: %s, Min battery: %d%%\n",
                 power.wake_on_rotation ? "ON" : "OFF",
                 power.min_battery_percent);

    // Update settings screen status
    uint8_t battery = M5.Power.getBatteryLevel();
    bool charging = M5.Power.isCharging();
    settingsScreen.updateStatus(battery, charging, false, "SET", 10, 45);

    Serial.println("\nControls:");
    Serial.println("- Touch 'Prev' / 'Next' to navigate pages (3 pages)");
    Serial.println("- Touch '<- Back' to save and exit");
    Serial.println("- Touch 'Reset' (page 2) to reset to defaults");
    Serial.println("- Touch sliders to adjust values");
    Serial.println("- Touch toggles to flip ON/OFF");
    Serial.println("\n[OK] Settings screen initialized\n");

    lastUpdate = millis();
    lastSecond = millis();
}

void loop() {
    M5.update();

    // Handle touch events
    auto touch = M5.Touch.getDetail();

    if (touch.wasPressed()) {
        settingsScreen.handleTouch(touch.x, touch.y, true);
    }

    if (touch.wasReleased()) {
        settingsScreen.handleTouch(touch.x, touch.y, false);
    }

    // Update at 30 FPS (~33ms per frame)
    uint32_t now = millis();
    uint32_t deltaMs = now - lastUpdate;

    if (deltaMs >= 33) {
        lastUpdate = now;

        // Update settings screen
        settingsScreen.update(deltaMs);

        // Draw
        settingsScreen.draw(renderer);

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

        settingsScreen.updateStatus(battery, charging, false, "SET", hour, minute);
    }

    delay(1);  // Prevent watchdog
}
