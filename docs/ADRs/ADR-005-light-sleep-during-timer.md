# ADR-005: Light Sleep Strategy During Active Timer

**Status**: Accepted

**Date**: 2025-12-15

**Decision Makers**: Development Team

## Context

The M5 Pomodoro Timer runs for 25-45 minute sessions, during which the user may not actively interact with the device. We need to decide whether and how to use ESP32 sleep modes during active timer sessions to extend battery life while maintaining timer accuracy and UI responsiveness.

### ESP32 Sleep Modes

| Mode | Power | Wake Time | Limitations |
|------|-------|-----------|-------------|
| **Active** | 80-200mA | N/A | No power saving |
| **Light Sleep** | 0.8mA | 1-5ms | CPU off, RTC on, RAM retained, touch/RTC wake |
| **Deep Sleep** | 0.15mA | 1-2s | Full reboot, only RTC memory retained, limited wake sources |
| **Modem Sleep** | 20-30mA | N/A | WiFi off, CPU on |

### Requirements

1. Timer must not drift (±1 second accuracy over 25 minutes)
2. UI must update every second (countdown display)
3. Gyro gestures must be detected (pause/resume)
4. Touch input must be responsive (<100ms)
5. Battery life target: 4+ hours continuous use

### Options Considered

1. **No Sleep (Always Active)**
   - Pros: Simple, no complexity, instant response
   - Cons: High power (~100mA), battery lasts ~3.9 hours (fails requirement)

2. **Deep Sleep Between UI Updates**
   - Pros: Ultra-low power (0.15mA)
   - Cons: 1-2s wake time, cannot detect gyro/touch, state persistence complex

3. **Light Sleep Between UI Updates**
   - Pros: Low power (0.8mA), fast wake (1-5ms), retains state, touch/RTC wake
   - Cons: CPU off, must wake every second for timer update

4. **Modem Sleep Only (WiFi Off)**
   - Pros: Simple, CPU stays on
   - Cons: Still high power (~30mA), only saves WiFi power

## Decision

**We will use Light Sleep between UI updates during active timer sessions, waking every 1 second via RTC timer:**

```
Timeline (1 second):
├─ [0ms]    Wake from light sleep
├─ [1-5ms]  Resume tasks, update timer (-1 second)
├─ [5-20ms] Render UI (timer display, progress bar)
├─ [20-30ms] Check for touch/gyro events
├─ [30ms]   Enter light sleep
└─ [1000ms] RTC alarm triggers wake → repeat
```

**Power Savings**: ~94% reduction during active timer
- Active: 100mA
- Light sleep: 0.8mA (average over 1s window with 30ms active: ~3.5mA)

### Rationale

1. **Battery Life**: Achieves 4+ hour target (390mAh / 3.5mA avg = ~11 hours theoretical, ~6-8 hours real-world)
2. **Timer Accuracy**: RTC timer is crystal-driven, ±20ppm accuracy (~±30 seconds per 25-minute session, corrected by NTP)
3. **Responsiveness**: 1-5ms wake time is imperceptible to user
4. **UI Updates**: 1 second wake interval maintains smooth countdown
5. **Gesture Detection**: Touch wake-up ensures gestures don't require waiting for RTC alarm
6. **State Retention**: RAM retained during light sleep, no complex persistence logic

## Implementation

### Light Sleep Configuration

```cpp
// Configure light sleep wake sources
void configureLightSleep() {
    // Wake source 1: RTC timer (1 second interval)
    esp_sleep_enable_timer_wakeup(1000000);  // 1 second in microseconds

    // Wake source 2: Touch screen (any touch)
    esp_sleep_enable_touchpad_wakeup();

    // Optional: External wakeup (power button)
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_37, 0);  // PWR button on Core2

    LOG_INFO("Light sleep configured: 1s RTC + touch wake");
}
```

### Timer Task with Light Sleep

```cpp
void timer_task_func(void* param) {
    TimerStateMachine stateMachine;
    uint32_t lastUpdate = millis();

    configureLightSleep();

    while (1) {
        // Check if timer is running
        if (stateMachine.isRunning()) {
            // Update timer (decrement by 1 second)
            stateMachine.tick();

            // Send update to UI task
            TimerUpdate update = {
                .state = stateMachine.getState(),
                .remainingSeconds = stateMachine.getRemainingSeconds(),
                .sessionNumber = stateMachine.getSessionNumber()
            };
            xQueueSend(timer_update_queue, &update, 0);

            // Process any pending events (gyro gestures, touch)
            GyroEvent gyroEvent;
            if (xQueueReceive(gyro_event_queue, &gyroEvent, 0) == pdTRUE) {
                stateMachine.handleGyroEvent(gyroEvent);
            }

            // Enter light sleep until next second
            // Wake sources: RTC timer (1s) OR touch
            uint32_t sleepDuration = 1000 - (millis() - lastUpdate);
            if (sleepDuration > 50 && sleepDuration < 1000) {
                esp_light_sleep_start();
            }

            lastUpdate = millis();
        } else {
            // Timer stopped, use longer sleep (5 seconds)
            esp_sleep_enable_timer_wakeup(5000000);  // 5s
            esp_light_sleep_start();
        }

        // Small delay to prevent tight loop (if sleep disabled)
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

### UI Task Wake on Timer Update

```cpp
void ui_task_func(void* param) {
    while (1) {
        TimerUpdate update;

        // Wait for timer update (blocking, 2s timeout)
        if (xQueueReceive(timer_update_queue, &update, pdMS_TO_TICKS(2000)) == pdTRUE) {
            // Update UI
            if (xSemaphoreTake(display_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                timerDisplay.render(160, 80, update.remainingSeconds);
                progressBar.render(20, 130, calculateProgress(update));
                xSemaphoreGive(display_mutex);
            }
        }

        // Handle touch events (independent of timer updates)
        TouchEvent touchEvent;
        if (xQueueReceive(touch_event_queue, &touchEvent, 0) == pdTRUE) {
            handleTouchEvent(touchEvent);
        }
    }
}
```

### Task Suspension During Sleep (Optional)

```cpp
void enterLightSleepWithSuspension() {
    // Suspend non-critical tasks before sleep
    vTaskSuspend(network_task_handle);   // Network not needed during timer
    vTaskSuspend(stats_task_handle);     // Stats calculated at session end
    vTaskSuspend(led_task_handle);       // LEDs off during sleep

    // Enter light sleep
    esp_light_sleep_start();

    // Resume tasks after wake
    vTaskResume(network_task_handle);
    vTaskResume(stats_task_handle);
    vTaskResume(led_task_handle);
}
```

**Additional Savings**: ~1-2mA (non-essential tasks suspended)

## Timer Accuracy Analysis

### RTC Drift

ESP32 RTC uses external 32.768 kHz crystal with ±20 ppm accuracy:

```
Drift calculation (25-minute Pomodoro):
25 minutes = 1500 seconds
Drift = 1500s × 20ppm = 0.03 seconds (30 milliseconds)

Worst case (over 8 hours):
8 hours = 28800 seconds
Drift = 28800s × 20ppm = 0.576 seconds (~0.6s)
```

**Conclusion**: Negligible drift (<1 second per 25-minute session). NTP sync every hour corrects any accumulated drift.

### NTP Drift Correction

```cpp
void handleNTPSync() {
    uint32_t oldEpoch = getCurrentEpoch();
    ntpClient.sync();
    uint32_t newEpoch = getCurrentEpoch();

    int32_t drift = newEpoch - oldEpoch;

    if (abs(drift) > 2 && timerStateMachine.isRunning()) {
        // Drift >2 seconds detected, adjust timer
        LOG_WARN("NTP drift detected: " + String(drift) + "s, adjusting timer");
        timerStateMachine.adjustTime(drift);
    }
}
```

**Mitigation**: NTP sync every hour ensures <2s drift accumulation.

## Power Budget

### Active Timer Session (25 minutes)

```
Component breakdown (light sleep strategy):

Active time (30ms per second):
- CPU (240MHz):           ~100mA × 30ms = 3.0mA avg
- Display (320×240 IPS):  ~40mA × 30ms = 1.2mA avg
- Gyro/Touch:             ~5mA × 30ms = 0.15mA avg
- Peripherals:            ~5mA × 30ms = 0.15mA avg

Sleep time (970ms per second):
- Light sleep:            0.8mA × 970ms = 0.776mA avg

Total average: 3.0 + 1.2 + 0.15 + 0.15 + 0.776 ≈ 5.3mA

Battery life:
390mAh / 5.3mA = 73.5 hours (unrealistic, assumes no other drains)

Real-world (accounting for network, peak draws):
390mAh / 15mA avg ≈ 26 hours
With periodic sync (Balanced mode): ~10-15 hours
Continuous sync (Performance mode): ~4-6 hours
```

**Target Met**: 4+ hours in Balanced mode ✅

### Comparison: No Sleep vs. Light Sleep

| Scenario | Avg Current | Battery Life |
|----------|-------------|--------------|
| No sleep (always active) | 100mA | 3.9 hours |
| Modem sleep (WiFi off) | 30mA | 13 hours |
| **Light sleep (1s wake)** | **15mA** | **26 hours** |
| Aggressive light sleep | 8mA | 48 hours |

## Consequences

### Positive

- **Extended Battery Life**: 94% power reduction during active timer, 4+ hour target exceeded
- **Timer Accuracy**: RTC drift <1s per session, corrected by NTP
- **Responsiveness**: 1-5ms wake time is imperceptible
- **Simplicity**: No deep sleep complexity, RAM retained, no state persistence needed
- **User Experience**: Smooth 1-second UI updates, instant touch response

### Negative

- **Implementation Complexity**: Requires careful coordination of wake sources and task suspension
- **Wake Overhead**: 30ms active time per second (3% duty cycle)
- **RTC Dependency**: Timer accuracy depends on RTC crystal quality (±20ppm typical)

### Neutral

- **CPU Idle Time**: 97% of the time CPU is in light sleep (good for battery, but underutilized)

## Testing Checklist

- [ ] Verify 1-second timer updates during light sleep
- [ ] Measure timer drift over 25-minute session (<1s)
- [ ] Measure timer drift over 8-hour period (<5s)
- [ ] Test touch wake from light sleep (<100ms latency)
- [ ] Test gyro wake from light sleep (if gyroWake enabled)
- [ ] Measure power consumption during active timer (target: <20mA avg)
- [ ] Verify NTP drift correction (adjust timer if drift >2s)
- [ ] Test task suspension/resumption (no crashes or hangs)
- [ ] Verify UI updates continue smoothly (no skipped frames)
- [ ] Test battery life (charge to 100%, run continuous 25-min sessions until depleted)

## Alternatives Revisited

### Why Not Deep Sleep?

Deep sleep would save an additional 0.65mA (0.8mA → 0.15mA), but:
1. **Wake Time**: 1-2 seconds (vs. 1-5ms) - unacceptable for 1-second UI updates
2. **State Loss**: Must save timer state to RTC memory before each sleep
3. **Wake Sources**: Limited (RTC timer + GPIO only, no touch wake on Core2)
4. **Complexity**: Full system reboot required on wake

**Conclusion**: Marginal power savings (~0.65mA) not worth 1-2s wake latency.

### Why Not Modem Sleep?

Modem sleep (WiFi off, CPU on) would be simpler, but:
1. **Power Consumption**: 30mA (vs. 5-15mA with light sleep)
2. **Battery Life**: ~13 hours (vs. ~26 hours with light sleep)
3. **No Significant Benefit**: WiFi already off during timer in Balanced mode

**Conclusion**: Light sleep provides 2× better battery life with minimal added complexity.

## Future Enhancements

### Adaptive Wake Interval

```cpp
// Wake every 1s for countdown updates, but skip display updates if screen off
if (screenBrightness > 0) {
    updateDisplay();
}
// Still update timer internally
```

### CPU Frequency Scaling

```cpp
// Reduce CPU frequency during sleep periods
setCpuFrequencyMhz(80);   // Lower frequency (80MHz)
performLightSleep();
setCpuFrequencyMhz(240);  // Restore (240MHz)
```

**Additional Savings**: ~2-3mA (CPU power scales with frequency)

## References

- ESP32 Sleep Modes: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/sleep_modes.html
- ESP32 RTC API: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/system_time.html
- Power Management: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/power_management.html

## Related ADRs

- ADR-004: Gyro Polling Strategy (gyro polling continues during light sleep)
- ADR-006: NVS Statistics Storage (no complex persistence needed)
- ADR-008: Power Mode Design (light sleep intervals vary by mode)

## Revision History

- 2025-12-15: Initial version, light sleep strategy for active timer
