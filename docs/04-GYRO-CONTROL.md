# Gyro Control System - M5 Pomodoro v2

## Document Information
- **Version**: 1.0
- **Date**: 2025-01-03
- **Status**: Draft
- **Related**: [02-ARCHITECTURE.md](02-ARCHITECTURE.md), [03-STATE-MACHINE.md](03-STATE-MACHINE.md)

---

## 1. Overview

The M5 Pomodoro v2 uses the MPU6886 6-axis IMU for gesture-based timer control. This document describes the gyroscope/accelerometer polling strategy, rotation detection algorithms, gesture recognition, and power-aware sampling.

**Key Constraint**: The MPU6886 interrupt pin is **NOT connected to GPIO** on the M5Stack Core2, requiring a polling-based approach instead of interrupt-driven wake.

---

## 2. Hardware Specifications

### 2.1 MPU6886 IMU

**Manufacturer**: TDK InvenSense
**Type**: 6-axis IMU (3-axis gyroscope + 3-axis accelerometer)
**Interface**: I2C @ 400kHz
**I2C Address**: 0x68
**Data Rate**: Configurable up to 1kHz
**Range**:
- Accelerometer: ±2g / ±4g / ±8g / ±16g (using ±4g for gestures)
- Gyroscope: ±250°/s / ±500°/s / ±1000°/s / ±2000°/s (using ±500°/s)

**Power Consumption**:
- Active mode: 3.5 mA (accel + gyro)
- Low-power mode: 0.5 mA (accel only)

### 2.2 M5Stack Core2 Integration

**I2C Bus**: Shared with other peripherals (HMI module, RTC)
- **SDA**: GPIO 21
- **SCL**: GPIO 22
- **INT Pin**: **NOT CONNECTED** to ESP32 GPIO (hardware limitation)

**Implications**:
- Cannot wake from deep sleep via motion detection
- Must poll MPU6886 periodically for data
- Need FreeRTOS mutex to serialize I2C bus access

---

## 3. Gesture Definitions

### 3.1 Rotation Gesture

**Purpose**: Start/pause timer by rotating device to edge

**Detection Criteria**:
- Rotation angle: ≥60° from vertical reference (configurable 30-90°)
- Time window: Change detected within 10 seconds
- Direction: Either clockwise or counterclockwise

**Physical Motion**:
```
      Vertical                Rotated 60°+
      (reference)             (gesture detected)
         │                         /
         │                       /
    ┌────┴────┐             ┌────/
    │         │             │   /
    │ M5Core2 │     →       │ M5Core2
    │         │             │
    └─────────┘             └─────────
      (Front)                 (Front)
```

**Use Cases**:
- From STOPPED: Rotation triggers START
- From POMODORO/REST: Rotation triggers PAUSE
- From PAUSED: Rotation ignored (use flat detection for resume)

### 3.2 Flat Detection

**Purpose**: Start next session by laying device flat (screen up)

**Detection Criteria**:
- Orientation: Horizontal (±10° from flat)
- Hold time: 1 second continuous (prevent accidental detection)
- Screen direction: Face up (Z-axis accelerometer > 0.9g)

**Physical Motion**:
```
      Upright               Flat on Table
                            (gesture detected)
    ┌─────────┐            ╔═══════════╗
    │         │            ║           ║
    │ M5Core2 │     →      ║  M5Core2  ║
    │         │            ║  (screen  ║
    └─────────┘            ╚═══up)═════╝
```

**Use Cases**:
- From SHORT_REST: Lay flat → Start next session
- From LONG_REST: Lay flat → Start new sequence (session 1)
- From PAUSED: Lay flat → Resume timer

### 3.3 Double-Shake Gesture

**Purpose**: Emergency stop from any state

**Detection Criteria**:
- Two distinct shakes within 1 second
- Shake threshold: >2g acceleration spike
- Direction: Any axis (X, Y, or Z)

**Physical Motion**:
```
Shake left-right-left rapidly:
    ←  →  ←
   (shake 1) (shake 2)
   <------ <1 sec ------>
```

**Use Cases**:
- Emergency stop timer from any state
- Provides haptic + visual confirmation
- Requires deliberate motion (unlikely to trigger accidentally)

---

## 4. Polling Strategy

### 4.1 Sampling Rates per Power Mode

| Power Mode | Sampling Rate | Purpose | CPU Load |
|------------|---------------|---------|----------|
| **PERFORMANCE** | 100 Hz (10ms) | Maximum responsiveness | ~3% Core 0 |
| **BALANCED** | 50 Hz (20ms) | Good responsiveness, lower power | ~1.5% Core 0 |
| **BATTERY_SAVER** | 25 Hz (40ms) | Acceptable latency, minimal power | ~0.7% Core 0 |
| **Light Sleep** | 0 Hz (paused) | Timer running, gyro idle | 0% |
| **Deep Sleep** | 0 Hz (off) | Gyro powered off, cannot wake | 0% |

### 4.2 Ticker-Based Polling

**Implementation** (Core 0):
```cpp
Ticker gyroTicker;

void setup() {
    // Start polling at 50 Hz (balanced mode default)
    gyroTicker.attach_ms(20, []{ gyroController.poll(); });
}

void setPowerMode(PowerMode mode) {
    gyroTicker.detach();
    switch(mode) {
        case PERFORMANCE:
            gyroTicker.attach_ms(10, []{ gyroController.poll(); });
            break;
        case BALANCED:
            gyroTicker.attach_ms(20, []{ gyroController.poll(); });
            break;
        case BATTERY_SAVER:
            gyroTicker.attach_ms(40, []{ gyroController.poll(); });
            break;
    }
}
```

### 4.3 I2C Bus Serialization

**Problem**: I2C bus shared between MPU6886, HMI module, and RTC

**Solution**: FreeRTOS mutex to protect I2C transactions
```cpp
SemaphoreHandle_t i2cMutex;

void GyroController::poll() {
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(10))) {
        // Read IMU data
        M5.Imu.getAccelData(&ax, &ay, &az);
        M5.Imu.getGyroData(&gx, &gy, &gz);

        xSemaphoreGive(i2cMutex);

        // Process data (outside mutex)
        processGestureData(ax, ay, az, gx, gy, gz);
    } else {
        // Mutex timeout, skip this sample
        missedSamples++;
    }
}
```

---

## 5. Rotation Detection Algorithm

### 5.1 Orientation Calculation

**Pitch and Roll from Accelerometer**:
```cpp
// Assuming ±4g range, accelerometer data in g
float pitch = atan2(ax, sqrt(ay*ay + az*az)) * 180.0 / PI;
float roll = atan2(ay, sqrt(ax*ax + az*az)) * 180.0 / PI;
```

**Coordinate System**:
```
      +Z (up, out of screen)
       │
       │
       │
       └────── +X (right)
      /
     /
   +Y (forward, away from user)
```

**Vertical Reference**:
- Device upright (portrait): pitch ≈ 0°, roll ≈ 0°
- Device rotated right: roll ≈ 90°
- Device rotated left: roll ≈ -90°

### 5.2 Rotation Detection Logic

**State Machine**:
```cpp
enum class RotationState {
    VERTICAL,      // Device upright, reference position
    ROTATING,      // Rotation in progress
    ROTATED,       // Threshold crossed, gesture detected
    COOLDOWN       // 500ms cooldown after gesture
};
```

**Detection Flow**:
```cpp
void detectRotation(float pitch, float roll) {
    float totalAngle = sqrt(pitch*pitch + roll*roll);

    switch (rotationState) {
        case VERTICAL:
            if (totalAngle > sensitivity_threshold) {
                // Rotation started
                rotationStartTime = millis();
                rotationState = ROTATING;
            }
            break;

        case ROTATING:
            if (totalAngle >= 60.0) { // Configurable threshold
                // Gesture detected!
                rotationState = ROTATED;
                onRotationDetected(roll > 0 ? RIGHT : LEFT);
            } else if (millis() - rotationStartTime > 10000) {
                // Timeout, user held partially rotated too long
                rotationState = VERTICAL;
            }
            break;

        case ROTATED:
            // Wait for user to return to vertical
            if (totalAngle < 20.0) {
                rotationState = COOLDOWN;
                cooldownStartTime = millis();
            }
            break;

        case COOLDOWN:
            if (millis() - cooldownStartTime > 500) {
                // Cooldown complete, ready for next gesture
                rotationState = VERTICAL;
            }
            break;
    }
}
```

### 5.3 Configurable Sensitivity

**User Setting**: Rotation threshold (30° to 90°)

**Purpose**:
- **Low threshold (30°)**: More sensitive, easier to trigger (risk of false positives)
- **High threshold (90°)**: Less sensitive, requires more rotation (more deliberate)

**Default**: 60° (good balance between convenience and accuracy)

```cpp
struct GyroConfig {
    uint8_t rotation_threshold = 60;  // degrees (30-90)
    bool enabled = true;
    // ... other settings
};
```

---

## 6. Flat Detection Algorithm

### 6.1 Horizontal Orientation Check

**Criteria**:
- Z-axis acceleration ≈ 1g (±0.1g tolerance)
- X-axis and Y-axis ≈ 0g (±0.2g tolerance)
- Screen facing up (Z > 0)

```cpp
bool isFlat() {
    // Check Z-axis (gravity pointing down through screen)
    bool zCorrect = (az > 0.9 && az < 1.1);

    // Check X and Y are near zero (device level)
    bool xyLevel = (abs(ax) < 0.2 && abs(ay) < 0.2);

    return zCorrect && xyLevel;
}
```

### 6.2 Hold Time Requirement

**Problem**: Momentary flat detection during movement

**Solution**: Require 1 second continuous flat orientation
```cpp
void detectFlat() {
    if (isFlat()) {
        if (flatStartTime == 0) {
            flatStartTime = millis();
        } else if (millis() - flatStartTime > 1000) {
            // Held flat for 1 second, gesture detected!
            onFlatDetected();
            flatStartTime = 0;
        }
    } else {
        // Not flat anymore, reset
        flatStartTime = 0;
    }
}
```

---

## 7. Shake Detection Algorithm

### 7.1 Acceleration Spike Detection

**Shake Definition**: Sudden change in acceleration magnitude

```cpp
// Calculate acceleration magnitude (vector length)
float accelMagnitude = sqrt(ax*ax + ay*ay + az*az);

// Detect spike (>2g total acceleration)
bool isShake = (accelMagnitude > 2.0);
```

### 7.2 Double-Shake Recognition

**State Machine**:
```cpp
enum class ShakeState {
    IDLE,           // Waiting for first shake
    FIRST_SHAKE,    // First shake detected, waiting for second
    COOLDOWN        // Gesture detected, 1-second cooldown
};
```

**Detection Logic**:
```cpp
void detectShake(float accelMag) {
    switch (shakeState) {
        case IDLE:
            if (accelMag > 2.0) {
                shakeState = FIRST_SHAKE;
                firstShakeTime = millis();
            }
            break;

        case FIRST_SHAKE:
            if (accelMag > 2.0 && millis() - firstShakeTime > 100) {
                // Second shake detected (>100ms after first, <1000ms)
                uint32_t timeBetween = millis() - firstShakeTime;
                if (timeBetween < 1000) {
                    onDoubleShakeDetected();
                    shakeState = COOLDOWN;
                    cooldownStart = millis();
                }
            } else if (millis() - firstShakeTime > 1000) {
                // Timeout, only one shake
                shakeState = IDLE;
            }
            break;

        case COOLDOWN:
            if (millis() - cooldownStart > 1000) {
                shakeState = IDLE;
            }
            break;
    }
}
```

---

## 8. Gesture Event Delivery

### 8.1 FreeRTOS Queue to State Machine

**Queue Structure**:
```cpp
enum class GyroGesture {
    NONE,
    ROTATE_RIGHT,
    ROTATE_LEFT,
    FLAT_UP,
    DOUBLE_SHAKE
};

struct GyroEvent {
    GyroGesture gesture;
    uint32_t timestamp;
    float angle;  // For rotation gestures
};

QueueHandle_t gyroEventQueue;  // Core 0, size 5
```

**Sending Events (Core 0, ISR-safe)**:
```cpp
void GyroController::onRotationDetected(GyroDirection dir) {
    GyroEvent event = {
        dir == RIGHT ? ROTATE_RIGHT : ROTATE_LEFT,
        millis(),
        currentAngle
    };
    xQueueSend(gyroEventQueue, &event, 0); // Non-blocking
}
```

**Receiving Events (Core 0, main loop)**:
```cpp
void loop() {
    GyroEvent event;
    if (xQueueReceive(gyroEventQueue, &event, 0)) {
        // Convert to timer event
        TimerEvent timerEvent;
        switch(event.gesture) {
            case ROTATE_RIGHT:
            case ROTATE_LEFT:
                timerEvent = GYRO_ROTATE;
                break;
            case FLAT_UP:
                timerEvent = GYRO_FLAT;
                break;
            case DOUBLE_SHAKE:
                timerEvent = GYRO_SHAKE;
                break;
        }
        stateMachine.handleEvent(timerEvent);
    }
}
```

### 8.2 Haptic Feedback on Detection

**Immediate Response**:
```cpp
void GyroController::onGestureDetected(GyroGesture gesture) {
    // Provide instant feedback before state machine processes
    switch(gesture) {
        case ROTATE_RIGHT:
        case ROTATE_LEFT:
            haptic.buzz(100); // 100ms vibration
            break;
        case FLAT_UP:
            haptic.buzz(50);  // 50ms vibration
            break;
        case DOUBLE_SHAKE:
            haptic.buzzPattern({200, 100, 200}); // Strong double buzz
            break;
    }

    // Send to queue
    xQueueSend(gyroEventQueue, &event, 0);
}
```

---

## 9. Calibration

### 9.1 Factory Calibration

**MPU6886 has built-in calibration**:
- Accelerometer: ±3% error typical
- Gyroscope: ±3% error typical
- No user calibration required for gesture detection (tolerances are wide)

### 9.2 Runtime Calibration (Optional Future Enhancement)

**Concept**: Allow user to calibrate vertical reference

**Procedure**:
1. User places device upright on flat surface
2. Press "Calibrate" in settings
3. Sample 100 readings over 2 seconds
4. Average → store as vertical reference
5. Use reference instead of 0° for rotation detection

**Benefit**: Compensates for manufacturing variations

---

## 10. Power Management

### 10.1 Adaptive Sampling

**Automatic Rate Adjustment**:
```cpp
void adjustGyroSamplingRate(PowerMode mode, uint8_t batteryPercent) {
    if (batteryPercent < 10) {
        // Critical battery, disable gyro
        gyroTicker.detach();
        gyroEnabled = false;
    } else {
        // Set rate based on power mode
        switch(mode) {
            case PERFORMANCE: gyroTicker.attach_ms(10, poll); break;
            case BALANCED:    gyroTicker.attach_ms(20, poll); break;
            case BATTERY_SAVER: gyroTicker.attach_ms(40, poll); break;
        }
    }
}
```

### 10.2 Sleep Mode Behavior

**Light Sleep (during timer)**:
- Gyro polling paused (ticker suspended)
- Wake on touch → resume polling
- Gesture detection unavailable during sleep

**Deep Sleep (after timeout)**:
- Gyro powered off completely
- Cannot wake via motion (hardware limitation)
- Must wake via touch or RTC timer

**Wake Procedure**:
```cpp
void onWakeFromSleep() {
    // Re-initialize MPU6886
    M5.Imu.Init();

    // Restart polling ticker
    gyroTicker.attach_ms(20, []{ gyroController.poll(); });

    // Reset state machines
    rotationState = VERTICAL;
    shakeState = IDLE;
}
```

---

## 11. Error Handling

### 11.1 I2C Communication Errors

**Symptoms**:
- xSemaphoreTake timeout (I2C bus busy)
- M5.Imu.getAccelData() returns NaN or 0
- Communication failure with MPU6886

**Recovery**:
```cpp
void GyroController::poll() {
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(10))) {
        int result = M5.Imu.getAccelData(&ax, &ay, &az);
        xSemaphoreGive(i2cMutex);

        if (result != 0) {
            i2cErrors++;
            if (i2cErrors > 10) {
                // Too many errors, try reinitializing
                logger.warn("Gyro: I2C errors, reinitializing...");
                M5.Imu.Init();
                i2cErrors = 0;
            }
            return; // Skip this sample
        }
        i2cErrors = 0; // Reset on success
    } else {
        mutexTimeouts++;
        // Bus busy, skip sample (acceptable, next poll in 20ms)
    }
}
```

### 11.2 False Positive Prevention

**Challenge**: Accidental gestures during normal handling

**Mitigations**:
1. **Rotation**: 60° threshold (deliberate motion), 500ms cooldown
2. **Flat**: 1-second hold time (must be intentional)
3. **Shake**: Two distinct shakes <1s apart (unlikely accidental)
4. **User disable**: Settings toggle to turn off gyro completely

**Logging for Tuning**:
```cpp
void logGestureStats() {
    logger.info("Gyro stats: %d rotations, %d flats, %d shakes, %d false positives",
        rotationCount, flatCount, shakeCount, falsePositiveCount);
}
```

---

## 12. Testing & Validation

### 12.1 Gesture Detection Tests

| Test ID | Gesture | Orientation | Expected Result | Pass/Fail |
|---------|---------|-------------|-----------------|-----------|
| G1 | Rotate right 60° | Portrait → Landscape right | ROTATE_RIGHT detected | |
| G2 | Rotate left 60° | Portrait → Landscape left | ROTATE_LEFT detected | |
| G3 | Rotate 45° (below threshold) | Portrait → 45° | No detection | |
| G4 | Lay flat screen up | Portrait → Flat | FLAT_UP after 1s | |
| G5 | Lay flat screen down | Portrait → Flat down | No detection | |
| G6 | Pick up from flat | Flat → Portrait | No detection | |
| G7 | Shake once | Any | No detection | |
| G8 | Shake twice <1s | Any | DOUBLE_SHAKE detected | |
| G9 | Shake twice >1s | Any | No detection | |
| G10 | Normal handling | Any | No false positives | |

### 12.2 Sampling Rate Tests

| Test ID | Power Mode | Sampling Rate | Latency | False Positives |
|---------|------------|---------------|---------|-----------------|
| S1 | PERFORMANCE | 100 Hz | <200ms | 0% |
| S2 | BALANCED | 50 Hz | <300ms | 0% |
| S3 | BATTERY_SAVER | 25 Hz | <500ms | 0% |
| S4 | Sleep → Wake | 0 Hz → 50 Hz | Resume <100ms | N/A |

### 12.3 I2C Contention Tests

| Test ID | Scenario | Expected Behavior |
|---------|----------|-------------------|
| I1 | Gyro + HMI simultaneous | Mutex serializes, no data corruption |
| I2 | 100 rapid gyro polls | <1% mutex timeouts |
| I3 | Gyro + RTC read conflict | One waits, both succeed |

---

## 13. Performance Metrics

### 13.1 Detection Latency

**Target**: <500ms from physical gesture to event delivery

**Measured** (estimated):
- Rotation detection: ~300ms (3-6 samples at 50 Hz)
- Flat detection: 1000ms + ~100ms (by design + processing)
- Shake detection: ~200ms (2-4 samples)

**Breakdown**:
```
Physical motion
    ↓ (sensor lag: ~10ms)
IMU data available
    ↓ (poll interval: 20ms average)
Sample read
    ↓ (processing: 5ms)
Gesture detected
    ↓ (queue send: <1ms)
Event queued
    ↓ (main loop: <10ms)
State machine handles
```

### 13.2 CPU Overhead

| Component | CPU Time per Sample | Sampling Rate | Total CPU % |
|-----------|---------------------|---------------|-------------|
| I2C read (accel + gyro) | 200µs | 50 Hz | 1.0% |
| Data processing | 100µs | 50 Hz | 0.5% |
| Total | 300µs | 50 Hz | **1.5%** Core 0 |

**Impact**: Negligible, plenty of headroom for UI rendering

### 13.3 Memory Footprint

```
GyroController class:      ~200 bytes
Event queue (5 events):    ~100 bytes
State machines:            ~50 bytes
Calibration data:          ~50 bytes
--------------------------------
Total:                     ~400 bytes
```

---

## 14. User Configuration

### 14.1 Settings Menu Options

**Gyro Enable/Disable**:
- Toggle: ON / OFF
- Default: ON
- Effect: Stops all polling, saves power

**Rotation Sensitivity**:
- Range: 30° to 90° (5° increments)
- Default: 60°
- Effect: Changes threshold for ROTATE gesture

**Gesture Confirmation** (future enhancement):
- Options: None / Haptic / Visual / Both
- Default: Both
- Effect: Feedback mode on gesture detection

### 14.2 Settings Persistence

```cpp
struct GyroConfig {
    bool enabled = true;
    uint8_t rotation_threshold = 60;  // degrees
    bool haptic_feedback = true;
};

// Save to NVS
preferences.begin("gyro_cfg", false);
preferences.putBool("enabled", config.enabled);
preferences.putUChar("threshold", config.rotation_threshold);
preferences.end();
```

---

## 15. Troubleshooting

### 15.1 Gesture Not Detected

**Symptoms**: Rotation/flat/shake gesture not triggering action

**Diagnostic Steps**:
1. Check settings: Gyro enabled?
2. Check power mode: Battery too low?
3. Check I2C errors: Look for communication failures in logs
4. Check state machine: Is device in state that accepts gesture?
5. Check sensitivity: Threshold too high?

**Solutions**:
- Enable gyro in settings
- Charge battery (>20%)
- Reinitialize IMU: Settings → Restart Gyro
- Lower sensitivity: Settings → Rotation Threshold → 45°

### 15.2 False Positives

**Symptoms**: Gestures triggering during normal handling

**Diagnostic Steps**:
1. Check sensitivity: Threshold too low?
2. Check false positive logs
3. Observe device motion when false positives occur

**Solutions**:
- Increase sensitivity: Settings → Rotation Threshold → 75°
- Disable gyro temporarily: Settings → Gyro → OFF
- Adjust hold times in code (flat: 1s → 1.5s)

### 15.3 High I2C Error Rate

**Symptoms**: Logs show frequent "I2C timeout" messages

**Diagnostic Steps**:
1. Check I2C bus traffic (RTC, HMI also using bus)
2. Check mutex timeout duration (currently 10ms)
3. Check sampling rate (too fast for bus capacity?)

**Solutions**:
- Reduce gyro sampling rate: BALANCED → BATTERY_SAVER
- Increase mutex timeout: 10ms → 20ms
- Disable HMI module if not used

---

## 16. Future Enhancements

### 16.1 Gesture Customization

**User-Defined Gestures**:
- Allow user to record custom gesture patterns
- Machine learning gesture recognition
- Gesture shortcuts (e.g., circle = skip rest)

### 16.2 Advanced Motion Detection

**Flip Detection**:
- Detect phone flip (screen down) → pause timer
- Useful for focus mode

**Tap Detection**:
- Double-tap on device → quick action
- Triple-tap → open menu

### 16.3 Orientation-Aware UI

**Auto-Rotate Screen**:
- Detect portrait vs landscape orientation
- Rotate UI accordingly (landscape = widescreen timer)

---

**Document Version**: 1.0
**Last Updated**: 2025-01-03
**Status**: APPROVED
**Implementation**: MP-8 (GyroController), MP-21 (Integration), MP-24 (Gesture events)
