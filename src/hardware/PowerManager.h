#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <M5Unified.h>
#include <cstdint>

/**
 * Power management via AXP192 PMIC (Power Management IC)
 *
 * Features:
 * - Battery level monitoring
 * - Charging state detection
 * - Sleep mode control (light/deep sleep)
 * - Display brightness control
 * - Power consumption optimization
 *
 * M5Stack Core2 uses AXP192 for:
 * - LiPo battery management (3.7V, 390mAh)
 * - USB-C charging (5V input)
 * - Power rail control (3.3V, 5V, display backlight)
 */
class PowerManager {
public:
    enum class SleepMode {
        NONE,        // Active mode
        LIGHT,       // CPU sleep, RAM retained, wake on timer/GPIO
        DEEP         // Deep sleep, only RTC active, wake on timer/reset
    };

    PowerManager();

    // Initialization
    bool begin();

    // Battery status
    uint8_t getBatteryLevel() const;           // 0-100%
    float getBatteryVoltage() const;            // Volts (3.0 - 4.2V typical)
    bool isCharging() const;
    bool isChargeFull() const;
    int16_t getBatteryCurrent() const;          // mA (negative = discharging)

    // Display brightness (via AXP192 LDO)
    void setBrightness(uint8_t percent);        // 0-100%
    uint8_t getBrightness() const;

    // Power modes
    void enterLightSleep(uint32_t duration_ms);
    void enterDeepSleep(uint32_t duration_sec);
    void wakeup();                              // Called after sleep wake

    // Power consumption
    float getPowerConsumption() const;          // Watts (estimated)
    uint32_t getEstimatedRuntime() const;       // Minutes remaining (if discharging)

    // Low battery protection
    bool isLowBattery() const;                  // < 10%
    bool isCriticalBattery() const;             // < 5%

    // Diagnostics
    void printStatus() const;

private:
    uint8_t current_brightness = 80;
    SleepMode current_sleep_mode = SleepMode::NONE;

    // Battery voltage to percentage conversion (LiPo curve)
    uint8_t voltageToPercent(float voltage) const;
};

#endif // POWER_MANAGER_H
