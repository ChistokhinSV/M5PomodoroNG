#include "PowerManager.h"
#include <Arduino.h>

PowerManager::PowerManager()
    : current_brightness(80),
      current_sleep_mode(SleepMode::NONE) {
}

bool PowerManager::begin() {
    // M5Unified already initialized M5.Power (AXP192)
    // Just set initial brightness
    setBrightness(current_brightness);

    Serial.println("[PowerManager] Initialized");
    printStatus();

    return true;
}

uint8_t PowerManager::getBatteryLevel() const {
    // M5.Power.getBatteryLevel() returns 0-100
    return M5.Power.getBatteryLevel();
}

float PowerManager::getBatteryVoltage() const {
    // Get battery voltage in millivolts, convert to volts
    return M5.Power.getBatteryVoltage() / 1000.0f;
}

bool PowerManager::isCharging() const {
    return M5.Power.isCharging();
}

bool PowerManager::isChargeFull() const {
    // Assume full if charging and battery >= 95%
    return isCharging() && getBatteryLevel() >= 95;
}

int16_t PowerManager::getBatteryCurrent() const {
    // M5Unified provides getBatteryCurrent() in mA
    // Negative = discharging, Positive = charging
    return M5.Power.getBatteryCurrent();
}

void PowerManager::setBrightness(uint8_t percent) {
    // Constrain to 0-100%
    current_brightness = constrain(percent, 0, 100);

    // M5.Display.setBrightness() expects 0-255
    uint8_t brightness_255 = map(current_brightness, 0, 100, 0, 255);
    M5.Display.setBrightness(brightness_255);
}

uint8_t PowerManager::getBrightness() const {
    return current_brightness;
}

void PowerManager::enterLightSleep(uint32_t duration_ms) {
    Serial.printf("[PowerManager] Entering light sleep for %lu ms\n", duration_ms);

    current_sleep_mode = SleepMode::LIGHT;

    // No need to touch the task watchdog: the CPU is paused during light sleep so the
    // WDT cannot tick, and UITask is not subscribed in the first place. A previous
    // esp_task_wdt_add(NULL) here ended up subscribing the task after wake without
    // adding the matching esp_task_wdt_reset() calls, causing a WDT abort ~5s later.
    //
    // M5Unified configures the correct touch-INT pin (GPIO 39 on M5Core2) for EXT0 wake
    // and sets the timer source. micro_seconds=0 means "wake on touch only".
    uint64_t micros = static_cast<uint64_t>(duration_ms) * 1000ULL;
    M5.Power.lightSleep(micros, true);

    current_sleep_mode = SleepMode::NONE;
    Serial.println("[PowerManager] Woke from light sleep");
}

void PowerManager::enterDeepSleep(uint32_t duration_sec) {
    Serial.printf("[PowerManager] Entering deep sleep for %lu seconds\n", duration_sec);
    Serial.flush();

    current_sleep_mode = SleepMode::DEEP;

    // Let M5Unified handle wake setup. On M5Core2 it enables EXT0 on GPIO 39 (touch INT)
    // and the timer source if requested. Doing it ourselves earlier was wrong: we set
    // EXT0 on GPIO 4, which M5Unified overrode anyway, and the only side-effect of our
    // setup was a stray gpio_deep_sleep_hold_en() that could latch unrelated pins.
    // Power-button wake is not supported here (M5.Power.deepSleep uses EXT0, single pin).
    uint64_t micros = static_cast<uint64_t>(duration_sec) * 1000000ULL;
    M5.Power.deepSleep(micros, true);

    // This code never executes (ESP32 resets after deep sleep wake)
}

void PowerManager::wakeup() {
    // Called after wake from sleep
    Serial.println("[PowerManager] Processing wakeup");

    // Restore display brightness
    setBrightness(current_brightness);

    // Print wake reason
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_TIMER:
            Serial.println("[PowerManager] Woke by timer");
            break;
        case ESP_SLEEP_WAKEUP_EXT0:
            Serial.println("[PowerManager] Woke by GPIO");
            break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            Serial.println("[PowerManager] Woke by touchpad");
            break;
        default:
            Serial.printf("[PowerManager] Woke by other: %d\n", wakeup_reason);
            break;
    }
}

float PowerManager::getPowerConsumption() const {
    // Estimate power consumption from battery current
    // P = V × I (Watts)
    float voltage = getBatteryVoltage();
    float current_amps = getBatteryCurrent() / 1000.0f;  // mA to A

    return voltage * abs(current_amps);  // Watts
}

uint32_t PowerManager::getEstimatedRuntime() const {
    if (isCharging()) {
        return 0;  // Unlimited when charging
    }

    int16_t current_ma = getBatteryCurrent();
    if (current_ma >= 0) {
        return 0;  // Not discharging
    }

    // Assume 390mAh battery capacity
    const uint16_t BATTERY_CAPACITY_MAH = 390;
    uint8_t battery_percent = getBatteryLevel();

    // Remaining capacity in mAh
    uint16_t remaining_mah = (BATTERY_CAPACITY_MAH * battery_percent) / 100;

    // Current drain rate (absolute value)
    uint16_t drain_ma = abs(current_ma);

    if (drain_ma == 0) {
        return 999;  // Very long time
    }

    // Time = Capacity / Current (hours), convert to minutes
    return (remaining_mah * 60) / drain_ma;
}

bool PowerManager::isLowBattery() const {
    return getBatteryLevel() <= 10 && !isCharging();
}

bool PowerManager::isCriticalBattery() const {
    return getBatteryLevel() <= 5 && !isCharging();
}

void PowerManager::printStatus() const {
    Serial.println("=== Power Status ===");
    Serial.printf("Battery: %d%% (%.2fV)\n", getBatteryLevel(), getBatteryVoltage());
    Serial.printf("Current: %d mA %s\n",
                  abs(getBatteryCurrent()),
                  getBatteryCurrent() < 0 ? "(discharging)" : "(charging)");
    Serial.printf("Charging: %s\n", isCharging() ? "YES" : "NO");
    Serial.printf("Brightness: %d%%\n", current_brightness);
    Serial.printf("Power: %.2f W\n", getPowerConsumption());

    if (!isCharging()) {
        uint32_t runtime = getEstimatedRuntime();
        Serial.printf("Est. Runtime: %lu min\n", runtime);
    }

    if (isLowBattery()) {
        Serial.println("⚠️ LOW BATTERY WARNING");
    }
    if (isCriticalBattery()) {
        Serial.println("🔴 CRITICAL BATTERY!");
    }

    Serial.println("===================");
}

uint8_t PowerManager::voltageToPercent(float voltage) const {
    // LiPo discharge curve approximation
    // 4.2V = 100%, 3.7V = 50%, 3.0V = 0%
    if (voltage >= 4.2f) return 100;
    if (voltage <= 3.0f) return 0;

    // Linear approximation (could use lookup table for accuracy)
    return (uint8_t)((voltage - 3.0f) / (4.2f - 3.0f) * 100.0f);
}
