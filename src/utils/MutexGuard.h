#ifndef MUTEX_GUARD_H
#define MUTEX_GUARD_H

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <Arduino.h>

/**
 * RAII Mutex Guard for FreeRTOS Semaphores
 *
 * Automatically acquires mutex on construction and releases on destruction.
 * Prevents forgetting to release mutexes and handles exceptions safely.
 *
 * Usage:
 *   {
 *       MutexGuard guard(g_i2c_mutex, "i2c_mutex", 100);
 *       if (guard.isLocked()) {
 *           // Critical section - mutex is held
 *           M5.Imu.getAccel(&ax, &ay, &az);
 *       } else {
 *           // Timeout - log error, don't access resource
 *           Serial.println("Failed to acquire I2C mutex");
 *       }
 *   }  // Mutex automatically released here
 *
 * Benefits:
 * - Exception-safe (mutex released even if exception thrown)
 * - No risk of forgetting to release
 * - Timeout enforced (never portMAX_DELAY)
 * - Logs timeout errors automatically
 */
class MutexGuard {
public:
    /**
     * Acquire mutex with timeout
     *
     * @param mutex FreeRTOS semaphore handle (must be mutex type)
     * @param name Mutex name for logging (e.g., "i2c_mutex")
     * @param timeout_ms Timeout in milliseconds (default 100ms)
     */
    MutexGuard(SemaphoreHandle_t mutex, const char* name, uint32_t timeout_ms = 100)
        : mutex_(mutex),
          name_(name),
          locked_(false) {

        if (mutex_ == NULL) {
            Serial.printf("[MutexGuard] ERROR: NULL mutex handle for %s\n", name_);
            return;
        }

        // Try to acquire mutex with timeout
        if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {
            locked_ = true;
        } else {
            // Timeout - log error
            Serial.printf("[MutexGuard] ERROR: Timeout acquiring %s (waited %lu ms)\n",
                         name_, timeout_ms);
            Serial.println("[MutexGuard] Potential deadlock - check lock ordering");
        }
    }

    /**
     * Release mutex on destruction
     *
     * Automatically called when MutexGuard goes out of scope.
     */
    ~MutexGuard() {
        if (locked_) {
            xSemaphoreGive(mutex_);
            locked_ = false;
        }
    }

    /**
     * Check if mutex was successfully acquired
     *
     * @return true if mutex is held, false if timeout occurred
     */
    bool isLocked() const {
        return locked_;
    }

    /**
     * Explicitly release mutex before destructor
     *
     * Useful if you need to release early within a scope.
     * Safe to call multiple times (no-op if already released).
     */
    void unlock() {
        if (locked_) {
            xSemaphoreGive(mutex_);
            locked_ = false;
        }
    }

    // Prevent copying (RAII objects should not be copied)
    MutexGuard(const MutexGuard&) = delete;
    MutexGuard& operator=(const MutexGuard&) = delete;

    // Allow moving (for returning from functions)
    MutexGuard(MutexGuard&& other) noexcept
        : mutex_(other.mutex_),
          name_(other.name_),
          locked_(other.locked_) {
        other.locked_ = false;  // Transfer ownership
    }

    MutexGuard& operator=(MutexGuard&& other) noexcept {
        if (this != &other) {
            // Release current mutex if held
            if (locked_) {
                xSemaphoreGive(mutex_);
            }

            // Transfer ownership
            mutex_ = other.mutex_;
            name_ = other.name_;
            locked_ = other.locked_;
            other.locked_ = false;
        }
        return *this;
    }

private:
    SemaphoreHandle_t mutex_;
    const char* name_;
    bool locked_;
};

/**
 * Helper macro for creating mutex guard with current task name
 *
 * Usage:
 *   MUTEX_GUARD(g_i2c_mutex, "i2c_mutex", 100);
 *   if (MUTEX_IS_LOCKED) {
 *       // Critical section
 *   }
 */
#define MUTEX_GUARD(mutex, name, timeout) \
    MutexGuard _mutex_guard_##__LINE__(mutex, name, timeout)

#define MUTEX_IS_LOCKED (_mutex_guard_##__LINE__.isLocked())

#endif // MUTEX_GUARD_H
