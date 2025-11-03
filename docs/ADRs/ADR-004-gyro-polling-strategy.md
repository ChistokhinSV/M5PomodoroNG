# ADR-004: Gyro Polling Strategy (No Interrupt Wake)

**Status**: Accepted

**Date**: 2025-12-15

**Decision Makers**: Development Team

## Context

The M5Stack Core2 uses the MPU6886 6-axis IMU (gyroscope + accelerometer) for gesture detection. We need to decide how to read gyro data to detect user gestures (rotate to start/pause, lay flat to resume, double-shake to stop).

### Hardware Constraint

**Critical Discovery**: The MPU6886 INT pin is **NOT connected to any GPIO** on M5Stack Core2. This means:
- Cannot use interrupt-driven wake from deep sleep
- Cannot use interrupt-driven gesture detection
- Must use polling strategy

### Options Considered

1. **Interrupt-Driven Gesture Detection**
   - Pros: Low CPU usage, instant response, can wake from deep sleep
   - Cons: **NOT POSSIBLE** - INT pin not connected on Core2

2. **Continuous Polling (No Sleep)**
   - Pros: Simple, consistent sampling rate, low latency
   - Cons: High power consumption (~100mA), drains battery in <4 hours

3. **Adaptive Polling with Light Sleep**
   - Pros: Power efficient, reasonable responsiveness, meets 4+ hour target
   - Cons: Higher latency, misses gestures during sleep, more complex

4. **Polling with Variable Sampling Rate**
   - Pros: Balances power and responsiveness, configurable per power mode
   - Cons: Some latency, requires careful tuning

## Decision

**We will use polling with variable sampling rate based on power mode, NO deep sleep gyro wake:**

| Power Mode | Sampling Rate | Task Delay | Power Impact |
|------------|---------------|------------|--------------|
| PERFORMANCE | 100 Hz | 10 ms | ~5 mA |
| BALANCED | 50 Hz | 20 ms | ~3 mA |
| BATTERY_SAVER | 25 Hz | 40 ms | ~2 mA |

**Light sleep**: Use during STOPPED state only (not during active timer)
**Wake trigger**: Touch screen or RTC alarm (not gyro)

### Rationale

1. **Hardware Limitation**: No alternative - INT pin not connected
2. **Battery Life**: Variable sampling reduces power consumption by 60% (5mA → 2mA in Battery Saver)
3. **Responsiveness**: 50Hz (Balanced) provides <20ms latency, acceptable for gesture detection
4. **Simplicity**: Polling is straightforward, no interrupt handling complexity
5. **Configurability**: User can prioritize responsiveness (Performance) or battery (Battery Saver)

### Gesture Detection Latency

```
PERFORMANCE mode:  10ms sampling + 60° threshold → ~100-200ms to detect rotation
BALANCED mode:     20ms sampling + 60° threshold → ~200-400ms to detect rotation
BATTERY_SAVER:     40ms sampling + 60° threshold → ~400-800ms to detect rotation
```

**Acceptable**: User rotation gestures take 1-2 seconds, 200-400ms latency is imperceptible.

## Implementation

### Gyro Task (Core 0)

```cpp
void gyro_task_func(void* param) {
    GyroController gyro;
    gyro.begin();

    while (1) {
        // Get current power mode
        PowerMode mode = getPowerMode();

        // Variable delay based on power mode
        uint16_t delayMs;
        switch (mode) {
            case PowerMode::PERFORMANCE:
                delayMs = 10;  // 100 Hz
                break;
            case PowerMode::BALANCED:
                delayMs = 20;  // 50 Hz
                break;
            case PowerMode::BATTERY_SAVER:
                delayMs = 40;  // 25 Hz
                break;
        }

        // Read gyro data (with I2C mutex)
        if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            float ax, ay, az, gx, gy, gz;
            M5.Imu.getAccel(&ax, &ay, &az);
            M5.Imu.getGyro(&gx, &gy, &gz);
            xSemaphoreGive(i2c_mutex);

            // Detect gestures
            GyroEvent event = gyro.detectGesture(ax, ay, az, gx, gy, gz);

            // Send to queue if gesture detected
            if (event.type != GyroEventType::NONE) {
                xQueueSend(gyro_event_queue, &event, 0);
            }
        }

        // Variable delay
        vTaskDelay(pdMS_TO_TICKS(delayMs));
    }
}
```

### Gesture Detection Algorithm

```cpp
// GyroController.cpp
GyroEvent GyroController::detectGesture(float ax, float ay, float az, float gx, float gy, float gz) {
    // 1. Rotation detection (±60° tilt)
    float pitch = atan2(ax, sqrt(ay*ay + az*az)) * 180.0 / M_PI;
    float roll = atan2(ay, sqrt(ax*ax + az*az)) * 180.0 / M_PI;
    float totalAngle = sqrt(pitch*pitch + roll*roll);

    if (totalAngle >= rotationThreshold && !rotationDetected) {
        rotationDetected = true;
        rotationStartTime = millis();
        return GyroEvent{GyroEventType::ROTATION, totalAngle};
    }

    // 2. Flat detection (lay flat for 1 second)
    bool isFlat = (az > 0.9 && az < 1.1) && (abs(ax) < 0.2 && abs(ay) < 0.2);

    if (isFlat) {
        if (!flatStarted) {
            flatStarted = true;
            flatStartTime = millis();
        } else if (millis() - flatStartTime > 1000) {
            // Flat for 1 second
            return GyroEvent{GyroEventType::FLAT, 0};
        }
    } else {
        flatStarted = false;
    }

    // 3. Double-shake detection
    float accelMagnitude = sqrt(ax*ax + ay*ay + az*az);
    bool isShake = (accelMagnitude > 2.0);  // 2G threshold

    if (isShake && !shakeDetected) {
        shakeDetected = true;
        shakeCount++;
        lastShakeTime = millis();

        if (shakeCount >= 2 && (millis() - firstShakeTime < 1000)) {
            // Double shake detected
            shakeCount = 0;
            return GyroEvent{GyroEventType::DOUBLE_SHAKE, 0};
        } else if (shakeCount == 1) {
            firstShakeTime = millis();
        }
    }

    // Reset shake detection after 1 second
    if (millis() - lastShakeTime > 1000) {
        shakeCount = 0;
        shakeDetected = false;
    }

    return GyroEvent{GyroEventType::NONE, 0};
}
```

### Power Mode Switching

```cpp
void setPowerMode(PowerMode mode) {
    currentPowerMode = mode;

    switch (mode) {
        case PowerMode::PERFORMANCE:
            // 100 Hz gyro sampling
            // No sleep
            LOG_INFO("Power mode: PERFORMANCE (100Hz gyro)");
            break;

        case PowerMode::BALANCED:
            // 50 Hz gyro sampling
            // Light sleep during STOPPED state
            LOG_INFO("Power mode: BALANCED (50Hz gyro)");
            break;

        case PowerMode::BATTERY_SAVER:
            // 25 Hz gyro sampling
            // Aggressive light sleep
            LOG_INFO("Power mode: BATTERY_SAVER (25Hz gyro)");
            break;
    }

    // Notify gyro task of mode change (no action needed, reads mode each iteration)
}
```

## Why Not Deep Sleep?

### Deep Sleep Limitations

1. **No Gyro Wake**: INT pin not connected → cannot wake on motion
2. **Cold Start**: Deep sleep requires full system reboot (~1-2 seconds)
3. **State Loss**: Must save timer state to RTC memory (complex)
4. **User Experience**: 1-2s wake delay unacceptable for gesture-driven timer

### Light Sleep Alternative

```cpp
void enterLightSleep(uint32_t durationMs) {
    // Configure wake sources
    esp_sleep_enable_timer_wakeup(durationMs * 1000);  // µs
    esp_sleep_enable_touchpad_wakeup();                // Touch screen wake

    // Suspend non-critical tasks
    vTaskSuspend(network_task_handle);
    vTaskSuspend(stats_task_handle);

    // Enter light sleep (CPU off, peripherals on)
    esp_light_sleep_start();

    // Resume tasks
    vTaskResume(network_task_handle);
    vTaskResume(stats_task_handle);

    LOG_INFO("Woke from light sleep");
}

// Use during STOPPED state only
void idle_manager() {
    if (timerState == PomodoroState::STOPPED && noUserActivityFor(30s)) {
        // Sleep for 5 seconds, wake on touch
        enterLightSleep(5000);
    }
}
```

**Power Savings**: Light sleep ~0.8mA (vs. ~5mA active, ~0.3mA deep sleep)

## Gesture Wake Workaround

**Goal**: Wake device from sleep when user rotates it (simulated gyro wake).

**Solution**: Aggressive light sleep with short wake intervals:

```cpp
void aggressiveLightSleep() {
    // Sleep for 1 second
    esp_sleep_enable_timer_wakeup(1000000);  // 1s in µs
    esp_light_sleep_start();

    // Wake up, poll gyro once
    float ax, ay, az;
    M5.Imu.getAccel(&ax, &ay, &az);

    float pitch = atan2(ax, sqrt(ay*ay + az*az)) * 180.0 / M_PI;
    float roll = atan2(ay, sqrt(ax*ax + az*az)) * 180.0 / M_PI;
    float totalAngle = sqrt(pitch*pitch + roll*roll);

    if (totalAngle >= 60.0) {
        // Motion detected, wake system
        wakeSystem();
        timerStateMachine.handleEvent(TimerEvent::START);
    } else {
        // No motion, go back to sleep
        aggressiveLightSleep();
    }
}
```

**Power Cost**: ~1mA average (vs. 0.3mA deep sleep, but functional gyro wake)

**User Experience**: <1s wake latency (vs. INT-based instant wake, but acceptable tradeoff)

**Configuration**: Enable/disable in settings (`gyroWake` boolean)

## Consequences

### Positive

- **Functional**: Polling works despite hardware limitation
- **Power Efficient**: Variable sampling reduces power by 60% in Battery Saver mode
- **Configurable**: User can tune responsiveness vs. battery life
- **Simple**: No interrupt handling complexity
- **Reliable**: Polling is deterministic and easy to debug

### Negative

- **Latency**: 200-400ms gesture detection latency in Balanced mode (vs. instant with interrupts)
- **Power Consumption**: Polling uses 2-5mA (vs. <0.1mA interrupt-driven)
- **No True Deep Sleep Wake**: Cannot wake from deep sleep via gyro (limited to light sleep)

### Neutral

- **Battery Life**: 4+ hour target still achievable with light sleep during idle

## Testing Checklist

- [ ] Verify 100Hz sampling in PERFORMANCE mode
- [ ] Verify 50Hz sampling in BALANCED mode
- [ ] Verify 25Hz sampling in BATTERY_SAVER mode
- [ ] Test rotation detection (±60°) with <500ms latency
- [ ] Test flat detection (1s hold) with <1.2s latency
- [ ] Test double-shake detection (<1s between shakes)
- [ ] Measure gyro task CPU usage (<5% in Balanced mode)
- [ ] Measure gyro task power consumption (target: 2-3mA in Balanced)
- [ ] Test I2C mutex contention with HMI module
- [ ] Test gesture wake from light sleep (if enabled)

## Alternatives Revisited

### Could We Use External Interrupt?

**No**. The INT pin is physically not connected to any GPIO pin on M5Stack Core2. Datasheet and schematic confirm this limitation. This is a hardware design decision by M5Stack to save GPIO pins for other peripherals.

### Could We Use a Different IMU?

**Not Practical**. The MPU6886 is soldered to the Core2 mainboard. Replacing it would require:
- Desoldering SMD component
- Soldering new IMU with INT connected to free GPIO
- Modifying M5Unified library

This is not viable for a production device.

## Future Enhancements

### Adaptive Sampling Rate

Dynamically adjust sampling rate based on detected motion:

```cpp
if (motionDetected) {
    samplingRate = 100;  // High rate for 5 seconds after motion
} else {
    samplingRate = 25;   // Low rate during idle
}
```

**Benefit**: Instant response when needed, power saving when idle.

### Gesture Filtering

Apply low-pass filter to reduce false positives:

```cpp
// Exponential moving average
filteredAx = alpha * ax + (1 - alpha) * filteredAx;
```

## References

- MPU6886 Datasheet: https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/datasheet/core/MPU-6886-000193%2Bv1.1_GHIC_en.pdf
- M5Stack Core2 Schematic: https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/schematic/Core/CORE2_V1.0_SCH.pdf
- ESP32 Light Sleep: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/sleep_modes.html

## Related ADRs

- ADR-002: Dual-Core Architecture (gyro task on Core 0)
- ADR-003: FreeRTOS Synchronization (i2c_mutex usage)
- ADR-005: Light Sleep During Timer (sleep strategy)

## Revision History

- 2025-12-15: Initial version, polling strategy due to INT pin limitation
