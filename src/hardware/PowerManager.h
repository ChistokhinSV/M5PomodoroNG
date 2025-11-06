#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include "IPowerManager.h"
#include <M5Unified.h>
#include <cstdint>

/**
 * Power management via AXP192 PMIC (Power Management IC)
 *
 * Implements IPowerManager interface for hardware abstraction (MP-49)
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
class PowerManager : public IPowerManager {
public:
    PowerManager();

    // Initialization
    bool begin() override;

    // Battery status
    uint8_t getBatteryLevel() const override;           // 0-100%
    float getBatteryVoltage() const override;            // Volts (3.0 - 4.2V typical)
    bool isCharging() const override;
    bool isChargeFull() const override;
    int16_t getBatteryCurrent() const override;          // mA (negative = discharging)

    // Display brightness (via AXP192 LDO)
    void setBrightness(uint8_t percent) override;        // 0-100%
    uint8_t getBrightness() const override;

    // Power modes
    void enterLightSleep(uint32_t duration_ms) override;
    void enterDeepSleep(uint32_t duration_sec) override;
    void wakeup() override;                              // Called after sleep wake

    // Power consumption
    float getPowerConsumption() const override;          // Watts (estimated)
    uint32_t getEstimatedRuntime() const override;       // Minutes remaining (if discharging)

    // Low battery protection
    bool isLowBattery() const override;                  // < 10%
    bool isCriticalBattery() const override;             // < 5%

    // Diagnostics
    void printStatus() const;

private:
    uint8_t current_brightness = 80;
    SleepMode current_sleep_mode = SleepMode::NONE;

    // Battery voltage to percentage conversion (LiPo curve)
    uint8_t voltageToPercent(float voltage) const;
};

#endif // POWER_MANAGER_H
