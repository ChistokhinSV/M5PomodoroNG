# Power Management - M5 Pomodoro v2

## Document Information
- **Version**: 1.0
- **Date**: 2025-01-03
- **Status**: Draft
- **Related**: [02-ARCHITECTURE.md](02-ARCHITECTURE.md), [01-TECHNICAL-REQUIREMENTS.md](01-TECHNICAL-REQUIREMENTS.md)

---

## 1. Overview

The M5 Pomodoro v2 prioritizes battery optimization for portable use. This document describes the AXP192 power management chip integration, three-tier power modes, sleep strategies, and WiFi duty cycling for maximum battery life.

**Target**: ≥4 hours runtime in balanced mode on 390mAh battery (or document actual + recommend external power)

---

## 2. Hardware Specifications

### 2.1 M5Stack Core2 Power System

**Power Management IC**: AXP192
**Battery**: 390mAh LiPo (3.7V nominal)
**Charging**: USB-C, 5V input, max 500mA charge current
**Power Rails**:
- 3.3V: Main logic (ESP32, display, peripherals)
- 5V: USB input
- VBUS: Switched 5V output
- LDO2: Display backlight (adjustable)
- LDO3: Vibration motor

### 2.2 Power Consumption Breakdown

| Component | Current Draw | Notes |
|-----------|--------------|-------|
| ESP32 (active, WiFi off) | ~80mA | Both cores running |
| ESP32 (light sleep) | ~0.8mA | CPU idle, RAM retained |
| ESP32 (deep sleep) | ~0.01mA | Only RTC active |
| Display (backlight 100%) | ~100mA | TFT LCD |
| Display (backlight 50%) | ~50mA | |
| Display (off) | ~5mA | Controller still powered |
| WiFi (active) | ~120mA | 2.4GHz radio, TX/RX |
| WiFi (power save) | ~20mA | DTIM beacon listening |
| LEDs (10× SK6812, 100%) | ~600mA | All white, full brightness |
| LEDs (10× SK6812, 20%) | ~120mA | Typical usage |
| MPU6886 (active) | ~3.5mA | Accel + gyro |
| Audio (I2S playback) | ~50mA | NS4168 amplifier |
| Vibration motor | ~80mA | When active |
| **Idle (typical)** | **~150mA** | Display 80%, WiFi periodic, gyro 50Hz |

### 2.3 Battery Life Calculations

**Capacity**: 390mAh (conservative, actual may be lower after aging)

**Theoretical Runtime**:
- **Performance mode** (~200mA avg): 390mAh / 200mA = **1.95 hours**
- **Balanced mode** (~100mA avg): 390mAh / 100mA = **3.9 hours** ✅ Target
- **Battery Saver** (~60mA avg): 390mAh / 60mA = **6.5 hours**

**Note**: Add 20% safety margin for battery degradation, inefficiency

---

## 3. Power Modes

### 3.1 Mode Definitions

#### PERFORMANCE Mode
**Purpose**: Maximum responsiveness, no compromises

| Parameter | Setting | Current Impact |
|-----------|---------|----------------|
| Screen brightness | 100% | 100mA |
| Gyro sampling | 100Hz | 3.5mA |
| WiFi | Always on | 120mA |
| MQTT keep-alive | 60s | Minimal |
| Render FPS | 10 FPS | 80mA (ESP32) |
| LED brightness | 50% | 60mA (when active) |
| NTP sync | Every 60s | Minimal |

**Total Average**: ~200mA
**Runtime**: ~2 hours

#### BALANCED Mode (Default)
**Purpose**: Good balance between features and battery life

| Parameter | Setting | Current Impact |
|-----------|---------|----------------|
| Screen brightness | 80% | 80mA |
| Gyro sampling | 50Hz | 3.5mA |
| WiFi | Periodic (120s) | ~30mA avg |
| MQTT keep-alive | 120s | Minimal |
| Render FPS | 10 FPS | 80mA (ESP32) |
| LED brightness | 20% | 24mA (when active) |
| NTP sync | Every 120s | Minimal |

**Total Average**: ~100mA
**Runtime**: ~4 hours ✅ Target

#### BATTERY_SAVER Mode
**Purpose**: Maximum battery life, acceptable performance

| Parameter | Setting | Current Impact |
|-----------|---------|----------------|
| Screen brightness | 50% | 50mA |
| Gyro sampling | 25Hz | 3.5mA |
| WiFi | On-demand only | ~10mA avg |
| MQTT keep-alive | 300s | Minimal |
| Render FPS | 4 FPS | 80mA (ESP32, less active) |
| LED brightness | 5% | 6mA (when active) |
| NTP sync | Every 600s | Minimal |

**Total Average**: ~60mA
**Runtime**: ~6.5 hours

### 3.2 Mode Switching

**Manual Selection** (Settings menu):
```cpp
void setPowerMode(PowerMode mode) {
    currentMode = mode;
    applyPowerSettings(mode);
    config.power_mode = mode; // Persist to NVS
    config.save();
}
```

**Automatic Switching** (battery level):
```cpp
void checkAutoModeSwitching() {
    uint8_t batteryPercent = powerManager.getBatteryPercent();

    if (batteryPercent < 20 && currentMode != BATTERY_SAVER && !manualOverride) {
        logger.info("Power: Battery <20%, switching to BATTERY_SAVER");
        setPowerMode(BATTERY_SAVER);
        ui.showToast("Battery Saver mode enabled");
    } else if (batteryPercent > 30 && isCharging() && currentMode == BATTERY_SAVER && !manualOverride) {
        logger.info("Power: Charging, restoring BALANCED mode");
        setPowerMode(BALANCED);
    }
}
```

**Manual Override**:
- User explicitly selects mode in settings → disable auto-switching
- Flag persists until user selects "Auto" mode

---

## 4. AXP192 Integration

### 4.1 Initialization

```cpp
void PowerManager::init() {
    // AXP192 is initialized by M5Unified
    M5.begin();

    // Configure LDO2 (display backlight) - adjustable
    M5.Power.setLDO2(3300); // 3.3V, on

    // Configure LDO3 (vibration motor)
    M5.Power.setLDO3(3300); // 3.3V, initially off

    // Configure charging current (default 190mA)
    M5.Power.setChargeCurrent(190); // mA

    // Enable battery ADC for monitoring
    M5.Power.setBatteryCharge(true);
}
```

### 4.2 Battery Monitoring

**Read Battery Voltage**:
```cpp
float PowerManager::getBatteryVoltage() {
    return M5.Power.getBatteryVoltage() / 1000.0; // Convert mV to V
}
```

**Calculate Battery Percentage** (non-linear LiPo curve):
```cpp
uint8_t PowerManager::getBatteryPercent() {
    float voltage = getBatteryVoltage();

    // LiPo discharge curve (typical)
    if (voltage >= 4.2) return 100;
    if (voltage >= 4.0) return 90 + (voltage - 4.0) * 50; // 4.0-4.2V = 90-100%
    if (voltage >= 3.8) return 70 + (voltage - 3.8) * 100; // 3.8-4.0V = 70-90%
    if (voltage >= 3.6) return 40 + (voltage - 3.6) * 150; // 3.6-3.8V = 40-70%
    if (voltage >= 3.4) return 10 + (voltage - 3.4) * 150; // 3.4-3.6V = 10-40%
    if (voltage >= 3.0) return (voltage - 3.0) * 25;       // 3.0-3.4V = 0-10%
    return 0; // <3.0V = critically low
}
```

**Check Charging Status**:
```cpp
bool PowerManager::isCharging() {
    return M5.Power.isCharging();
}
```

**Battery Update Ticker** (every 30 seconds):
```cpp
void updateBattery() {
    batteryPercent = powerManager.getBatteryPercent();
    isCharging = powerManager.isCharging();

    // Update UI
    statusBar.setBatteryIcon(batteryPercent, isCharging);

    // Check auto mode switching
    checkAutoModeSwitching();
}
```

### 4.3 Backlight Control

**Adjust Brightness per Power Mode**:
```cpp
void PowerManager::setScreenBrightness(uint8_t percent) {
    // Map percent (0-100) to LDO2 voltage (2500-3300mV)
    // Higher voltage = brighter backlight
    uint16_t voltage = 2500 + (percent * 8); // 0% = 2500mV, 100% = 3300mV
    M5.Power.setLDO2(voltage);
}
```

**Apply per Mode**:
```cpp
void applyPowerSettings(PowerMode mode) {
    switch(mode) {
        case PERFORMANCE:
            setScreenBrightness(100);
            break;
        case BALANCED:
            setScreenBrightness(80);
            break;
        case BATTERY_SAVER:
            setScreenBrightness(50);
            break;
    }
}
```

### 4.4 Vibration Motor Control

```cpp
void PowerManager::vibratePattern(std::vector<uint16_t> pattern) {
    for (uint16_t duration : pattern) {
        if (duration > 0) {
            M5.Power.setLDO3(3300); // Turn on
            delay(duration);
            M5.Power.setLDO3(0); // Turn off
        } else {
            delay(50); // Pause between vibrations
        }
    }
}
```

---

## 5. WiFi Power Management

### 5.1 WiFi Modes per Power Mode

#### PERFORMANCE: Always On
```cpp
void setupWiFiPerformance() {
    WiFi.begin(ssid, password);
    // Keep WiFi always active, no power save
    WiFi.setSleep(WIFI_PS_NONE);

    // MQTT persistent connection
    mqttClient.setKeepAlive(60);
}
```
**Benefit**: Instant cloud sync, no reconnection delay
**Cost**: ~120mA continuous

#### BALANCED: Periodic Connect
```cpp
void setupWiFiBalanced() {
    // Connect → sync → disconnect cycle every 120s
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(WIFI_PS_MIN_MODEM); // Light sleep between beacons

    // Ticker: connect every 2 minutes
    wifiTicker.attach(120, []{ wifiManager.connectAndSync(); });
}

void connectAndSync() {
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.begin(ssid, password);
        // Wait up to 10 seconds
        for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
            delay(500);
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        // Publish queued shadow updates
        shadowSync.publishQueue();

        // Sync NTP if needed
        timeManager.syncIfNeeded();

        // Disconnect to save power
        WiFi.disconnect();
    }
}
```
**Benefit**: ~70% power savings vs always-on
**Cost**: Shadow updates delayed up to 120s

#### BATTERY_SAVER: On-Demand Only
```cpp
void setupWiFiBatterySaver() {
    // WiFi off by default
    WiFi.mode(WIFI_OFF);

    // Only connect on state change events
}

void onStateChange(PomodoroState newState) {
    // Connect briefly to publish critical updates
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
        delay(100);
    }

    if (WiFi.status() == WL_CONNECTED) {
        shadowSync.publishState(newState);
        delay(1000); // Ensure MQTT published
        WiFi.disconnect();
    }
}
```
**Benefit**: ~90% power savings vs always-on
**Cost**: Only syncs on important events, may miss cloud commands

### 5.2 WiFi Power Save Mode

**DTIM Beacon Interval**:
```cpp
// Set WiFi power save mode (when in BALANCED)
WiFi.setSleep(WIFI_PS_MIN_MODEM);
// ESP32 wakes every DTIM beacon (typically 3× beacon interval = 300ms)
// Reduces WiFi power from 120mA to ~20mA
```

**Trade-off**: Slight latency increase (up to 300ms), acceptable for Pomodoro sync

---

## 6. Sleep Modes

### 6.1 Light Sleep

**Purpose**: Reduce power during timer operation without stopping functionality

**When Used**:
- Timer running (POMODORO or REST state)
- User inactive (no touch, gyro idle)

**What Stays Active**:
- FreeRTOS tickers (1-second timer countdown)
- Touch interrupt (wake on touch)
- WiFi (if PERFORMANCE mode)

**What Sleeps**:
- CPU idle (automatic between tickers)
- Display remains on (no sleep for screen)

**Implementation**:
```cpp
void enterLightSleep() {
    // Configure wake sources
    esp_sleep_enable_timer_wakeup(1000000); // Wake every 1 second for timer tick
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_39, 0); // Wake on touch (TP_INT)

    // Enter light sleep (returns after 1s or touch)
    esp_light_sleep_start();

    // Wakes here, FreeRTOS continues
}
```

**Power Savings**: CPU idle between tickers (~0.8mA), but display still ~50-100mA

**Note**: Not heavily used since display is always on during timer

### 6.2 Deep Sleep

**Purpose**: Maximum power savings during inactivity

**When Used**:
- Timer STOPPED or PAUSED
- Inactivity timeout exceeded (configurable 0-600s, default 300s)

**What Stays Active**:
- RTC (BM8563) for timekeeping
- Touch interrupt circuit (wake source)
- Battery monitoring (AXP192)

**What Powers Off**:
- ESP32 (except RTC controller)
- Display (backlight off, controller powered down)
- WiFi/Bluetooth
- All peripherals

**Implementation**:
```cpp
void enterDeepSleep(uint32_t timeout_s) {
    // Save state to RTC memory (survives deep sleep)
    saveStateToRTC();

    // Configure wake sources
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_39, 0); // Touch interrupt
    if (timeout_s > 0) {
        esp_sleep_enable_timer_wakeup(timeout_s * 1000000ULL); // RTC timer wake
    }

    // Power down display
    M5.Power.setLDO2(0);

    // Display "Sleeping..." message
    M5.Display.drawString("Device sleeping, touch to wake", 100, 120);
    delay(2000);
    M5.Display.sleep();

    // Enter deep sleep
    logger.info("Power: Entering deep sleep for %d seconds", timeout_s);
    esp_deep_sleep_start();

    // NEVER RETURNS - device resets on wake
}
```

**Wake Procedure**:
```cpp
void onWakeFromDeepSleep() {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    switch(wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0:
            logger.info("Power: Woke from touch");
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            logger.info("Power: Woke from RTC timer");
            break;
        default:
            logger.info("Power: Cold boot (not from sleep)");
            break;
    }

    // Restore state from RTC memory
    restoreStateFromRTC();

    // Reinitialize hardware
    M5.begin();
    M5.Display.wakeup();
    M5.Power.setLDO2(3300); // Backlight on

    // Check if timer expired during sleep
    if (timerWasRunning && hasExpired()) {
        stateMachine.handleEvent(EXPIRE);
    }
}
```

**Power Consumption**: ~0.3mA (AXP192 ~0.26mA + RTC ~0.01mA + leakage)

**Wake Latency**: ~2-3 seconds (full ESP32 boot)

### 6.3 State Persistence (RTC Memory)

**RTC Memory** (8KB, survives deep sleep):
```cpp
RTC_DATA_ATTR struct {
    bool was_running;           // Timer was active before sleep
    PomodoroState state;        // Current state
    uint32_t start_epoch;       // Timer start timestamp
    uint16_t duration_seconds;  // Timer duration
    uint16_t remaining_seconds; // If paused
    PomodoroMode mode;          // Sequence mode
    uint8_t current_session;    // Classic mode session number
} rtcState;

void saveStateToRTC() {
    rtcState.was_running = (state == POMODORO || state == SHORT_REST || state == LONG_REST);
    rtcState.state = state;
    rtcState.start_epoch = start_epoch;
    rtcState.duration_seconds = duration_seconds;
    rtcState.remaining_seconds = remaining_seconds;
    rtcState.mode = pomodoroSequence.mode;
    rtcState.current_session = pomodoroSequence.current_session;
}

void restoreStateFromRTC() {
    if (rtcState.was_running) {
        state = rtcState.state;
        start_epoch = rtcState.start_epoch;
        duration_seconds = rtcState.duration_seconds;
        // ... restore all fields
    }
}
```

---

## 7. Component Power Control

### 7.1 LED Strip Power Management

**Adaptive Brightness**:
```cpp
void LEDController::setBrightnessForMode(PowerMode mode) {
    switch(mode) {
        case PERFORMANCE:
            FastLED.setBrightness(128); // 50%
            break;
        case BALANCED:
            FastLED.setBrightness(51);  // 20%
            break;
        case BATTERY_SAVER:
            FastLED.setBrightness(13);  // 5%
            break;
    }
    FastLED.show();
}
```

**LED Off When Idle**:
```cpp
void onStateChanged(PomodoroState newState) {
    if (newState == STOPPED && config.auto_led_off) {
        ledController.off();
    } else {
        ledController.setStateAnimation(newState);
    }
}
```

### 7.2 Gyro Sampling Rate

**Adaptive Polling** (covered in 04-GYRO-CONTROL.md):
```cpp
void setGyroSamplingRate(PowerMode mode) {
    gyroTicker.detach();
    switch(mode) {
        case PERFORMANCE: gyroTicker.attach_ms(10, poll); break; // 100Hz
        case BALANCED:    gyroTicker.attach_ms(20, poll); break; // 50Hz
        case BATTERY_SAVER: gyroTicker.attach_ms(40, poll); break; // 25Hz
    }
}
```

**Disable Gyro at Critical Battery**:
```cpp
if (batteryPercent < 10) {
    gyroTicker.detach();
    gyroEnabled = false;
    ui.showToast("Gyro disabled (low battery)");
}
```

### 7.3 Render Frame Rate

**Adaptive FPS**:
```cpp
void setRenderFPS(PowerMode mode) {
    renderTicker.detach();
    switch(mode) {
        case PERFORMANCE:
        case BALANCED:
            renderTicker.attach_ms(100, render); // 10 FPS
            break;
        case BATTERY_SAVER:
            renderTicker.attach_ms(250, render); // 4 FPS
            break;
    }
}
```

---

## 8. Power Monitoring & Logging

### 8.1 Current Draw Measurement (Optional Future Enhancement)

**AXP192 Capability**: Read discharge/charge current

```cpp
float PowerManager::getCurrentDraw() {
    // AXP192 can measure battery current
    return M5.Power.getBatteryCurrent(); // mA (positive = discharge, negative = charge)
}
```

**Data Logging**:
```cpp
void logPowerStats() {
    float voltage = getBatteryVoltage();
    float current = getCurrentDraw();
    uint8_t percent = getBatteryPercent();

    logger.info("Power: %d%% (%.2fV, %.0fmA)", percent, voltage, current);
}
```

**Use Case**: Measure actual power draw in each mode, tune settings

### 8.2 Battery Life Estimation

**Remaining Time Calculation**:
```cpp
uint16_t PowerManager::estimateRemainingMinutes() {
    uint8_t percent = getBatteryPercent();
    float avgCurrent = getAverageCurrentDraw(); // mA, estimated or measured

    if (avgCurrent <= 0) return 0; // Charging or error

    float remainingCapacity = 390.0 * (percent / 100.0); // mAh
    float remainingHours = remainingCapacity / avgCurrent;

    return (uint16_t)(remainingHours * 60); // Convert to minutes
}
```

**Display** (status bar):
```
Battery: 65% (2h 30m remaining)
```

### 8.3 Power Events

**Low Battery Warning**:
```cpp
void checkLowBattery() {
    if (batteryPercent <= 20 && !lowBatteryWarned) {
        ui.showToast("Low battery (20%)", 5000);
        lowBatteryWarned = true;
    } else if (batteryPercent <= 10) {
        ui.showToast("Critical battery (10%)!", 5000);
        // Auto-enable BATTERY_SAVER
        setPowerMode(BATTERY_SAVER);
    } else if (batteryPercent <= 5) {
        // Emergency deep sleep to protect battery
        logger.warn("Power: Critical battery, entering emergency sleep");
        enterDeepSleep(3600); // Sleep 1 hour
    }
}
```

**Charging Detected**:
```cpp
void onChargingStarted() {
    logger.info("Power: Charging started");
    ui.showToast("Charging...");

    // Optionally restore BALANCED mode if was in BATTERY_SAVER
    if (currentMode == BATTERY_SAVER && !manualOverride) {
        setPowerMode(BALANCED);
    }
}
```

---

## 9. Battery Life Testing

### 9.1 Test Procedure

**Setup**:
1. Fully charge device (100%)
2. Configure power mode (PERFORMANCE, BALANCED, or BATTERY_SAVER)
3. Start timer (continuous pomodoro sessions with auto-start)
4. Monitor battery level every 15 minutes
5. Record time when battery reaches 0% (device shuts off)

**Data Collection**:
```cpp
struct BatteryTestLog {
    uint32_t timestamp_epoch;
    uint8_t battery_percent;
    float voltage;
    float current_draw; // If measurable
    PomodoroState state;
    bool wifi_connected;
};
```

**Log to SD Card or Serial**:
```cpp
void logBatteryTest() {
    BatteryTestLog log = {
        rtc.getEpoch(),
        powerManager.getBatteryPercent(),
        powerManager.getBatteryVoltage(),
        powerManager.getCurrentDraw(),
        stateMachine.getState(),
        WiFi.isConnected()
    };

    File file = SD.open("/battery_test.csv", FILE_APPEND);
    file.printf("%d,%d,%.2f,%.0f,%d,%d\n",
        log.timestamp_epoch, log.battery_percent, log.voltage,
        log.current_draw, log.state, log.wifi_connected);
    file.close();
}
```

### 9.2 Expected Results

**Target** (BALANCED mode):
- Runtime: ≥4 hours
- Average current: ~100mA
- Display on entire time, WiFi periodic, gyro 50Hz

**Acceptable Range**: 3.5 - 4.5 hours (battery variation, usage patterns)

**If Below Target**: Document actual runtime, add note in user guide "For extended use, connect USB power"

### 9.3 Test Scenarios

| Scenario | Config | Expected Runtime |
|----------|--------|------------------|
| 1. Continuous pomodoros (auto-start) | BALANCED | 4 hours |
| 2. Manual start/stop (more idle time) | BALANCED | 5 hours |
| 3. WiFi always on | PERFORMANCE | 2 hours |
| 4. WiFi off (no cloud) | BATTERY_SAVER | 6.5 hours |
| 5. Screen off (audio-only) | Custom | 8+ hours |
| 6. Deep sleep (standby) | Auto sleep | 5+ days |

---

## 10. Troubleshooting

### 10.1 Battery Drains Quickly

**Symptoms**: Device runtime <2 hours in BALANCED mode

**Diagnostic Steps**:
1. Check power mode: Settings → Power → Current mode
2. Check WiFi status: Always connected? (should be periodic in BALANCED)
3. Check LED brightness: Settings → LED brightness (should be 20% in BALANCED)
4. Check gyro sampling: Settings → Gyro (50Hz in BALANCED)
5. Check screen brightness: Settings → Screen brightness (80% in BALANCED)
6. Check battery health: May be degraded if device old

**Solutions**:
- Switch to BATTERY_SAVER mode manually
- Disable gyro: Settings → Gyro → OFF
- Reduce LED brightness: Settings → LED → 0%
- Reduce screen brightness: Settings → Screen → 50%
- Disable WiFi: Turn on airplane mode (if cloud sync not needed)
- Replace battery (if device >2 years old)

### 10.2 Device Doesn't Wake from Deep Sleep

**Symptoms**: Screen stays black after deep sleep, unresponsive

**Diagnostic Steps**:
1. Try touch screen multiple times (may need firm press)
2. Try pressing physical power button (side of device)
3. Check battery: May be fully drained

**Solutions**:
- Connect USB-C power, wait 10 seconds, try waking again
- Force reboot: Hold power button 10 seconds
- Disable deep sleep: Settings → Sleep Timeout → Never (0s)

### 10.3 Battery Percentage Incorrect

**Symptoms**: Percentage jumps (e.g., 50% → 20% suddenly) or shows 100% when clearly lower

**Causes**:
- Non-linear LiPo discharge curve (steep drop at end)
- Battery degradation (capacity reduced)
- AXP192 calibration drift

**Solutions**:
- Perform full discharge/charge cycle (0% → 100%) to recalibrate
- Accept that final 20% drains faster (normal LiPo behavior)
- If persistently wrong, battery may need replacement

---

## 11. Future Enhancements

### 11.1 Adaptive Power Mode

**Machine Learning**: Learn user usage patterns, predict optimal power mode

```cpp
// Collect usage data
struct UsagePattern {
    uint8_t hour_of_day;       // 0-23
    uint8_t day_of_week;       // 0-6 (Mon-Sun)
    uint16_t typical_duration; // Minutes of use
    PowerMode preferred_mode;
};

// Auto-switch based on learned patterns
void adaptivePowerMode() {
    UsagePattern pattern = predictCurrentPattern();
    setPowerMode(pattern.preferred_mode);
}
```

### 11.2 Solar Charging Support

**Concept**: External solar panel charging via USB-C

**Implementation**: Detect solar charging (lower current), adjust expectations

### 11.3 Wireless Charging

**Concept**: Qi wireless charging pad (requires hardware mod)

**Note**: M5Core2 does not natively support wireless charging

### 11.4 Power Profiles

**User-Defined Profiles**:
- "Focus": WiFi off, LEDs off, screen dim, gyro sensitive
- "Connected": WiFi always on, full brightness, gyro off
- "Night": Screen very dim, no audio, vibrate only

---

## 12. Power Budget Summary

### 12.1 Component Power Table

| Component | Performance | Balanced | Battery Saver |
|-----------|------------|----------|---------------|
| ESP32 | 80mA | 80mA | 80mA |
| Display (backlight) | 100mA | 80mA | 50mA |
| WiFi | 120mA | 30mA avg | 10mA avg |
| Gyro (polling) | 3.5mA | 3.5mA | 3.5mA |
| LEDs (average) | 60mA | 24mA | 6mA |
| Audio (occasional) | 10mA avg | 10mA avg | 10mA avg |
| Misc (vibration, etc.) | 5mA avg | 5mA avg | 5mA avg |
| **Total** | **~380mA** | **~230mA** | **~165mA** |

**Note**: Total includes occasional peak usage (LEDs, audio). Continuous average lower.

**Adjusted Average** (accounting for idle periods):
- PERFORMANCE: ~200mA
- BALANCED: ~100mA
- BATTERY_SAVER: ~60mA

### 12.2 Runtime Estimates

| Power Mode | Current | Capacity | Runtime | Tested |
|------------|---------|----------|---------|--------|
| PERFORMANCE | 200mA | 390mAh | 1.95h | TBD |
| BALANCED | 100mA | 390mAh | 3.9h | TBD ✅ Target |
| BATTERY_SAVER | 60mA | 390mAh | 6.5h | TBD |
| Deep Sleep (standby) | 0.3mA | 390mAh | 1300h (54 days) | TBD |

**After Testing**: Update this table with measured runtimes (MP-31, MP-32)

---

**Document Version**: 1.0
**Last Updated**: 2025-01-03
**Status**: DRAFT (pending battery tests)
**Implementation**: MP-9 (PowerManager), MP-29 (Power modes), MP-30 (Sleep), MP-31 (Battery testing)
