/*
 * M5 Pomodoro Timer v2 - Widget Library Test
 *
 * Testing all widgets: Button, StatusBar, ProgressBar, SequenceIndicator, StatsChart
 */

#include <M5Unified.h>
#include "ui/Renderer.h"
#include "ui/widgets/Widget.h"
#include "ui/widgets/Button.h"
#include "ui/widgets/StatusBar.h"
#include "ui/widgets/ProgressBar.h"
#include "ui/widgets/SequenceIndicator.h"
#include "ui/widgets/StatsChart.h"

Renderer renderer;
uint32_t lastUpdate = 0;
uint32_t lastSecond = 0;

// Test widgets
StatusBar statusBar;
ProgressBar progressBar;
SequenceIndicator sequenceIndicator;
StatsChart statsChart;
Button button1, button2, button3;

// Test state
uint8_t progress = 0;
uint8_t currentSession = 0;
uint8_t completedSessions = 0;
bool increasing = true;
bool chartVisible = false;

void onButton1Press() {
    Serial.println("[Button] Button 1 pressed - Toggle chart");
    chartVisible = !chartVisible;
    statsChart.setVisible(chartVisible);
}

void onButton2Press() {
    Serial.println("[Button] Button 2 pressed - Reset progress");
    progress = 0;
    progressBar.setProgress(progress);
}

void onButton3Press() {
    Serial.println("[Button] Button 3 pressed - Next session");
    if (currentSession < 15) {
        completedSessions = currentSession;
        currentSession++;
        sequenceIndicator.setSession(currentSession, completedSessions);
    } else {
        currentSession = 0;
        completedSessions = 0;
        sequenceIndicator.setSession(0, 0);
    }
}

void setup() {
    // Initialize Serial
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n=================================");
    Serial.println("M5 Pomodoro v2 - Widget Test");
    Serial.println("=================================");

    // Initialize M5Unified
    auto cfg = M5.config();
    cfg.clear_display = true;
    cfg.internal_imu = false;
    cfg.internal_rtc = false;
    cfg.internal_spk = false;
    cfg.internal_mic = false;
    M5.begin(cfg);

    Serial.println("[OK] M5Unified initialized");

    // Check PSRAM
    if (psramFound()) {
        Serial.printf("[OK] PSRAM detected: %d bytes total, %d bytes free\n",
                      ESP.getPsramSize(), ESP.getFreePsram());
    } else {
        Serial.println("[WARNING] PSRAM not detected!");
    }

    // Initialize Renderer
    if (!renderer.begin()) {
        Serial.println("[ERROR] Renderer initialization failed!");
        while (1) delay(100);
    }

    Serial.println("[OK] Renderer initialized");
    Serial.println("\nWidget Test Controls:");
    Serial.println("- Button 1: Toggle chart visibility");
    Serial.println("- Button 2: Reset progress");
    Serial.println("- Button 3: Next session\n");

    // Initialize widgets
    // StatusBar at top
    statusBar.setBounds(0, 0, 320, 20);
    statusBar.updateBattery(M5.Power.getBatteryLevel(), false);
    statusBar.updateWiFi(false);
    statusBar.updateMode("BAL");
    statusBar.updateTime(10, 45);

    // ProgressBar
    progressBar.setBounds(20, 40, 280, 20);
    progressBar.setColor(Renderer::Color(TFT_RED));
    progressBar.setProgress(0);

    // SequenceIndicator
    sequenceIndicator.setBounds(60, 75, 200, 20);
    sequenceIndicator.setSession(0, 0);

    // Buttons
    button1.setBounds(20, 110, 70, 40);
    button1.setLabel("CHART");
    button1.setCallback(onButton1Press);

    button2.setBounds(125, 110, 70, 40);
    button2.setLabel("RESET");
    button2.setCallback(onButton2Press);

    button3.setBounds(230, 110, 70, 40);
    button3.setLabel("NEXT");
    button3.setCallback(onButton3Press);

    // StatsChart (hidden initially)
    statsChart.setBounds(20, 160, 280, 70);
    statsChart.setVisible(false);
    uint8_t testData[7] = {3, 5, 4, 7, 6, 2, 4};
    statsChart.setData(testData);

    Serial.println("[OK] Widgets initialized");

    lastUpdate = millis();
    lastSecond = millis();
}

void loop() {
    M5.update();

    // Handle touch events
    auto touch = M5.Touch.getDetail();
    if (touch.wasPressed()) {
        int16_t x = touch.x;
        int16_t y = touch.y;

        // Check button hits
        if (button1.hitTest(x, y)) {
            button1.onTouch(x, y);
        } else if (button2.hitTest(x, y)) {
            button2.onTouch(x, y);
        } else if (button3.hitTest(x, y)) {
            button3.onTouch(x, y);
        }
    }

    if (touch.wasReleased()) {
        int16_t x = touch.x;
        int16_t y = touch.y;

        // Release all buttons
        button1.onRelease(x, y);
        button2.onRelease(x, y);
        button3.onRelease(x, y);
    }

    // Update at 30 FPS (~33ms per frame)
    uint32_t now = millis();
    uint32_t deltaMs = now - lastUpdate;

    if (deltaMs >= 33) {
        lastUpdate = now;

        // Update progress bar animation
        progressBar.update(deltaMs);
        sequenceIndicator.update(deltaMs);

        // Clear screen
        renderer.clear(Renderer::Color(TFT_BLACK));

        // Draw all widgets
        statusBar.draw(renderer);
        progressBar.draw(renderer);
        sequenceIndicator.draw(renderer);
        button1.draw(renderer);
        button2.draw(renderer);
        button3.draw(renderer);

        if (chartVisible) {
            statsChart.draw(renderer);
        }

        // Push updates to screen
        renderer.update();
    }

    // Update test values every second
    if (now - lastSecond >= 1000) {
        lastSecond = now;

        // Animate progress
        if (increasing) {
            progress += 5;
            if (progress >= 100) {
                progress = 100;
                increasing = false;
            }
        } else {
            if (progress >= 5) {
                progress -= 5;
            } else {
                progress = 0;
                increasing = true;
            }
        }
        progressBar.setProgress(progress);

        // Update status bar time
        static uint8_t minute = 45;
        static uint8_t hour = 10;
        minute++;
        if (minute >= 60) {
            minute = 0;
            hour = (hour + 1) % 24;
        }
        statusBar.updateTime(hour, minute);

        // Update battery (simulate drain)
        static uint8_t battery = 100;
        if (battery > 0) battery--;
        statusBar.updateBattery(battery, false);
    }

    delay(1);  // Prevent watchdog
}
