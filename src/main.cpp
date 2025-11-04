/*
 * M5 Pomodoro Timer v2 - Statistics Screen Test
 *
 * Testing StatsScreen with populated Statistics data
 */

#include <M5Unified.h>
#include "ui/Renderer.h"
#include "ui/screens/StatsScreen.h"
#include "core/Statistics.h"

Renderer renderer;
uint32_t lastUpdate = 0;
uint32_t lastSecond = 0;

// Core components
Statistics statistics;

// Stats screen
StatsScreen statsScreen(statistics);

void setup() {
    // Initialize M5
    auto cfg = M5.config();
    M5.begin(cfg);

    Serial.begin(115200);
    delay(100);

    Serial.println("\n=================================");
    Serial.println("M5 Pomodoro v2 - Stats Screen Test");
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

    // Initialize statistics system
    if (!statistics.begin()) {
        Serial.println("[ERROR] Failed to initialize statistics");
    } else {
        Serial.println("[OK] Statistics initialized");
    }

    // Populate statistics with test data
    Serial.println("[Test] Populating statistics with test data...");

    // Record several work sessions for today
    for (uint8_t s = 0; s < 6; s++) {
        statistics.recordWorkSession(25, true);  // 25 min, completed
    }

    // Print stats for verification
    Statistics::DayStats last7[7];
    statistics.getLast7Days(last7);

    Serial.print("[Test] Last 7 days: [");
    for (uint8_t i = 0; i < 7; i++) {
        Serial.print(last7[i].completed_sessions);
        if (i < 6) Serial.print(", ");
    }
    Serial.println("]");

    Statistics::DayStats today = statistics.getToday();
    Serial.printf("[Test] Today: %d\n", today.completed_sessions);
    Serial.printf("[Test] Total: %d\n", statistics.getTotalCompleted());
    Serial.printf("[Test] Last 7 days total: %d\n", statistics.getLast7DaysTotal());

    // Update stats screen status
    uint8_t battery = M5.Power.getBatteryLevel();
    bool charging = M5.Power.isCharging();
    statsScreen.updateStatus(battery, charging, false, "BAL", 10, 45);

    Serial.println("\nControls:");
    Serial.println("- Touch '<- Back' button (top-left)");
    Serial.println("\n[OK] Stats screen initialized\n");

    lastUpdate = millis();
    lastSecond = millis();
}

void loop() {
    M5.update();

    // Handle touch events
    auto touch = M5.Touch.getDetail();

    if (touch.wasPressed()) {
        statsScreen.handleTouch(touch.x, touch.y, true);
    }

    if (touch.wasReleased()) {
        statsScreen.handleTouch(touch.x, touch.y, false);
    }

    // Update at 30 FPS (~33ms per frame)
    uint32_t now = millis();
    uint32_t deltaMs = now - lastUpdate;

    if (deltaMs >= 33) {
        lastUpdate = now;

        // Update stats screen
        statsScreen.update(deltaMs);

        // Draw
        statsScreen.draw(renderer);

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

        statsScreen.updateStatus(battery, charging, false, "BAL", hour, minute);
    }

    delay(1);  // Prevent watchdog
}
