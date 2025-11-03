/*
 * M5 Pomodoro Timer v2
 * 
 * A productivity timer for M5Stack Core2 implementing the Pomodoro Technique
 * with gyro-based gesture controls, cloud sync, and advanced power management.
 * 
 * Platform: M5Stack Core2 (ESP32-D0WDQ6-V3)
 * Framework: Arduino (ESP-IDF)
 * 
 * See docs/ for comprehensive documentation
 */

#include <M5Unified.h>

void setup() {
    // Initialize M5Unified
    auto cfg = M5.config();
    cfg.clear_display = true;
    cfg.internal_imu = true;
    cfg.internal_rtc = true;
    cfg.internal_spk = false;
    cfg.internal_mic = false;
    M5.begin(cfg);

    // Display startup message
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setTextDatum(MC_DATUM);
    M5.Display.setFont(&fonts::Font4);
    M5.Display.drawString("M5 Pomodoro Timer v2", 160, 100);
    M5.Display.drawString("Initializing...", 160, 130);

    Serial.begin(115200);
    Serial.println("M5 Pomodoro Timer v2 - " FIRMWARE_VERSION);
    Serial.println("Initialization complete");

    M5.Display.clear();
    M5.Display.drawString("Ready!", 160, 120);
}

void loop() {
    M5.update();
    delay(10);
}
