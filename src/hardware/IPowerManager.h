#ifndef I_POWER_MANAGER_H
#define I_POWER_MANAGER_H

#include <cstdint>

/**
 * Power Manager Interface - Hardware Abstraction for Testing (MP-49)
 *
 * Provides abstract interface for power management, allowing:
 * - Unit testing with mock implementations
 * - Hardware-independent power tests
 * - Dependency injection for better testability
 *
 * Usage:
 *   // Production code uses concrete PowerManager
 *   IPowerManager* power = new PowerManager();
 *   uint8_t battery = power->getBatteryLevel();
 *   if (power->isLowBattery()) {
 *       power->setBrightness(30);  // Reduce brightness
 *   }
 *
 *   // Test code uses MockPowerManager
 *   MockPowerManager mockPower;
 *   mockPower.setBatteryLevel(15);  // Simulate low battery
 *   // ... verify low battery handling
 */
class IPowerManager {
public:
    /**
     * Sleep modes for power optimization
     */
    enum class SleepMode {
        NONE,        // Active mode
        LIGHT,       // CPU sleep, RAM retained, wake on timer/GPIO
        DEEP         // Deep sleep, only RTC active, wake on timer/reset
    };

    virtual ~IPowerManager() = default;

    // ====================
    // Initialization
    // ====================

    /**
     * Initialize power management hardware (AXP192 PMIC)
     * @return true if successful, false on failure
     */
    virtual bool begin() = 0;

    // ====================
    // Battery Status
    // ====================

    /**
     * Get battery level percentage
     * @return Battery level (0-100%)
     */
    virtual uint8_t getBatteryLevel() const = 0;

    /**
     * Get battery voltage
     * @return Voltage in volts (3.0 - 4.2V typical for LiPo)
     */
    virtual float getBatteryVoltage() const = 0;

    /**
     * Check if battery is charging
     * @return true if charging, false otherwise
     */
    virtual bool isCharging() const = 0;

    /**
     * Check if battery is fully charged
     * @return true if charge complete, false otherwise
     */
    virtual bool isChargeFull() const = 0;

    /**
     * Get battery current
     * @return Current in milliamps (negative = discharging, positive = charging)
     */
    virtual int16_t getBatteryCurrent() const = 0;

    // ====================
    // Display Brightness Control
    // ====================

    /**
     * Set display brightness
     * @param percent Brightness level (0-100%)
     */
    virtual void setBrightness(uint8_t percent) = 0;

    /**
     * Get current brightness level
     * @return Brightness (0-100%)
     */
    virtual uint8_t getBrightness() const = 0;

    // ====================
    // Power Modes (Sleep)
    // ====================

    /**
     * Enter light sleep mode (CPU sleep, RAM retained)
     * @param duration_ms Sleep duration in milliseconds
     */
    virtual void enterLightSleep(uint32_t duration_ms) = 0;

    /**
     * Enter deep sleep mode (only RTC active)
     * @param duration_sec Sleep duration in seconds
     */
    virtual void enterDeepSleep(uint32_t duration_sec) = 0;

    /**
     * Wake from sleep mode
     * Called after timer/GPIO wakeup to restore normal operation
     */
    virtual void wakeup() = 0;

    // ====================
    // Power Consumption
    // ====================

    /**
     * Get estimated power consumption
     * @return Power usage in watts
     */
    virtual float getPowerConsumption() const = 0;

    /**
     * Get estimated runtime on battery
     * @return Estimated minutes remaining (0 if charging or invalid)
     */
    virtual uint32_t getEstimatedRuntime() const = 0;

    // ====================
    // Low Battery Protection
    // ====================

    /**
     * Check if battery is low (< 10%)
     * @return true if battery low, false otherwise
     */
    virtual bool isLowBattery() const = 0;

    /**
     * Check if battery is critically low (< 5%)
     * @return true if battery critical, false otherwise
     */
    virtual bool isCriticalBattery() const = 0;
};

#endif // I_POWER_MANAGER_H
