#ifndef MOCK_POWER_MANAGER_H
#define MOCK_POWER_MANAGER_H

#include "../../src/hardware/IPowerManager.h"

/**
 * Mock Power Manager for Unit Testing (MP-49)
 *
 * Provides fake implementation of IPowerManager for testing without hardware.
 *
 * Features:
 * - Tracks all method calls for verification
 * - Simulates battery state and power consumption
 * - Allows test injection of battery/charging state
 * - No actual AXP192 PMIC hardware access
 *
 * Usage in Tests:
 *   MockPowerManager power;
 *   power.setBatteryLevel(15);     // Simulate low battery
 *   power.setCharging(false);
 *
 *   ASSERT_TRUE(power.isLowBattery());
 *   ASSERT_EQ(15, power.getBatteryLevel());
 */
class MockPowerManager : public IPowerManager {
public:
    MockPowerManager() = default;

    // ========================================
    // IPowerManager Interface Implementation
    // ========================================

    bool begin() override {
        begin_called_ = true;
        return begin_result_;
    }

    uint8_t getBatteryLevel() const override {
        return battery_level_;
    }

    float getBatteryVoltage() const override {
        return battery_voltage_;
    }

    bool isCharging() const override {
        return charging_;
    }

    bool isChargeFull() const override {
        return charge_full_;
    }

    int16_t getBatteryCurrent() const override {
        return battery_current_;
    }

    void setBrightness(uint8_t percent) override {
        brightness_ = (percent > 100) ? 100 : percent;
        set_brightness_count_++;
    }

    uint8_t getBrightness() const override {
        return brightness_;
    }

    void enterLightSleep(uint32_t duration_ms) override {
        last_light_sleep_duration_ = duration_ms;
        light_sleep_count_++;
        current_sleep_mode_ = SleepMode::LIGHT;
    }

    void enterDeepSleep(uint32_t duration_sec) override {
        last_deep_sleep_duration_ = duration_sec;
        deep_sleep_count_++;
        current_sleep_mode_ = SleepMode::DEEP;
    }

    void wakeup() override {
        wakeup_count_++;
        current_sleep_mode_ = SleepMode::NONE;
    }

    float getPowerConsumption() const override {
        return power_consumption_;
    }

    uint32_t getEstimatedRuntime() const override {
        return estimated_runtime_;
    }

    bool isLowBattery() const override {
        return battery_level_ < 10;
    }

    bool isCriticalBattery() const override {
        return battery_level_ < 5;
    }

    // ========================================
    // Test Simulation Methods
    // ========================================

    /**
     * Set battery level for testing
     * @param percent Battery percentage (0-100%)
     */
    void setBatteryLevel(uint8_t percent) {
        battery_level_ = (percent > 100) ? 100 : percent;
        // Simulate voltage based on LiPo discharge curve
        battery_voltage_ = 3.0f + (battery_level_ / 100.0f) * 1.2f;  // 3.0V-4.2V
    }

    /**
     * Set battery voltage for testing
     */
    void setBatteryVoltage(float voltage) {
        battery_voltage_ = voltage;
    }

    /**
     * Set charging state for testing
     */
    void setCharging(bool charging) {
        charging_ = charging;
    }

    /**
     * Set charge full state for testing
     */
    void setChargeFull(bool full) {
        charge_full_ = full;
    }

    /**
     * Set battery current for testing
     * @param current_ma Current in milliamps (negative = discharging, positive = charging)
     */
    void setBatteryCurrent(int16_t current_ma) {
        battery_current_ = current_ma;
    }

    /**
     * Set power consumption for testing
     */
    void setPowerConsumption(float watts) {
        power_consumption_ = watts;
    }

    /**
     * Set estimated runtime for testing
     */
    void setEstimatedRuntime(uint32_t minutes) {
        estimated_runtime_ = minutes;
    }

    // ========================================
    // Test Inspection Methods
    // ========================================

    /**
     * Check if begin() was called
     */
    bool beginCalled() const { return begin_called_; }

    /**
     * Get current sleep mode
     */
    SleepMode currentSleepMode() const { return current_sleep_mode_; }

    /**
     * Get last light sleep duration (ms)
     */
    uint32_t lastLightSleepDuration() const { return last_light_sleep_duration_; }

    /**
     * Get last deep sleep duration (seconds)
     */
    uint32_t lastDeepSleepDuration() const { return last_deep_sleep_duration_; }

    /**
     * Get number of setBrightness() calls
     */
    int setBrightnessCount() const { return set_brightness_count_; }

    /**
     * Get number of enterLightSleep() calls
     */
    int lightSleepCount() const { return light_sleep_count_; }

    /**
     * Get number of enterDeepSleep() calls
     */
    int deepSleepCount() const { return deep_sleep_count_; }

    /**
     * Get number of wakeup() calls
     */
    int wakeupCount() const { return wakeup_count_; }

    /**
     * Reset all state for next test
     */
    void reset() {
        begin_called_ = false;
        battery_level_ = 100;
        battery_voltage_ = 4.2f;
        charging_ = false;
        charge_full_ = false;
        battery_current_ = 0;
        brightness_ = 80;
        current_sleep_mode_ = SleepMode::NONE;
        power_consumption_ = 0.5f;
        estimated_runtime_ = 300;

        last_light_sleep_duration_ = 0;
        last_deep_sleep_duration_ = 0;

        set_brightness_count_ = 0;
        light_sleep_count_ = 0;
        deep_sleep_count_ = 0;
        wakeup_count_ = 0;
    }

    /**
     * Configure begin() to return specific result
     */
    void setBeginResult(bool result) {
        begin_result_ = result;
    }

private:
    // State
    bool begin_called_ = false;
    bool begin_result_ = true;
    uint8_t battery_level_ = 100;
    float battery_voltage_ = 4.2f;
    bool charging_ = false;
    bool charge_full_ = false;
    int16_t battery_current_ = 0;
    uint8_t brightness_ = 80;
    SleepMode current_sleep_mode_ = SleepMode::NONE;
    float power_consumption_ = 0.5f;  // Watts
    uint32_t estimated_runtime_ = 300;  // Minutes

    // Last values
    uint32_t last_light_sleep_duration_ = 0;
    uint32_t last_deep_sleep_duration_ = 0;

    // Call counters
    int set_brightness_count_ = 0;
    int light_sleep_count_ = 0;
    int deep_sleep_count_ = 0;
    int wakeup_count_ = 0;
};

#endif // MOCK_POWER_MANAGER_H
