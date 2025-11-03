# ADR-002: Dual-Core FreeRTOS Architecture

**Status**: Accepted

**Date**: 2025-12-15

**Decision Makers**: Development Team

## Context

The ESP32-D0WDQ6-V3 in M5Stack Core2 has two CPU cores (Xtensa LX6) running at 240MHz. M5 Pomodoro Timer v1 used a single-core architecture with blocking operations in the main loop, which caused UI lag during network operations and inefficient CPU utilization.

For v2, we need to decide how to structure the application to take advantage of both cores while maintaining stability and minimizing complexity.

### Options Considered

1. **Single-Core with Cooperative Multitasking**
   - Pros: Simpler code, no synchronization needed, easier debugging
   - Cons: UI lags during network operations, inefficient CPU use, blocking operations

2. **Dual-Core with Core Pinning (FreeRTOS)**
   - Pros: True parallelism, UI remains responsive during network ops, better power efficiency
   - Cons: Requires synchronization primitives, potential race conditions, more complex

3. **Dual-Core with Work-Stealing Scheduler**
   - Pros: Automatic load balancing, optimal CPU utilization
   - Cons: Unpredictable task placement, difficult to debug, overkill for this application

## Decision

**We will use a dual-core FreeRTOS architecture with explicit core pinning:**

- **Core 0 (Protocol CPU)**: UI rendering, touch input, gyro polling, LED control, audio playback
- **Core 1 (Application CPU)**: Timer logic, cloud sync, network operations, statistics calculations

### Rationale

1. **Separation of Concerns**: Core 0 handles time-critical UI/input tasks, Core 1 handles background processing
2. **Responsiveness**: UI never blocks on network operations (AWS IoT sync can take 5-10 seconds)
3. **Power Efficiency**: Core 1 can enter idle state when not syncing, reducing power consumption
4. **Predictability**: Explicit core pinning ensures deterministic behavior (critical for timer accuracy)
5. **ESP32 Best Practice**: Official ESP-IDF recommends Core 0 for WiFi/Bluetooth tasks, Core 1 for application logic
6. **FreeRTOS Integration**: M5Unified and ESP-IDF already use FreeRTOS, minimal overhead

### Task Allocation

```
Core 0 (Protocol CPU):
├─ ui_task (priority 2)           - Main UI rendering loop
├─ touch_task (priority 3)        - Touch input processing
├─ gyro_task (priority 3)         - Gyro gesture detection
├─ led_task (priority 1)          - LED pattern updates
└─ audio_task (priority 1)        - Audio playback

Core 1 (Application CPU):
├─ timer_task (priority 4)        - Timer state machine (highest priority)
├─ network_task (priority 2)      - Cloud sync, MQTT
├─ stats_task (priority 1)        - Statistics calculations
└─ idle_task (priority 0)         - FreeRTOS idle task
```

**Priority**: Higher number = higher priority (FreeRTOS convention)

### Inter-Task Communication

```
Queues (FIFO):
- touch_event_queue: TouchTask → UITask (button presses)
- gyro_event_queue: GyroTask → TimerTask (gestures)
- timer_update_queue: TimerTask → UITask (countdown updates)
- cloud_sync_queue: TimerTask → NetworkTask (state changes)

Semaphores (Mutual Exclusion):
- i2c_mutex: Serializes I2C access (gyro, HMI module share I2C bus)
- display_mutex: Serializes display access (sprites, UI updates)
- stats_mutex: Protects statistics data structure

Event Groups (Synchronization):
- timer_events: START, PAUSE, RESUME, STOP, EXPIRE
- network_events: CONNECTED, DISCONNECTED, SYNCED
```

### Memory Allocation

- **Core 0 Tasks**: Stack allocated from internal DRAM (520KB SRAM)
  - ui_task: 8KB stack
  - touch_task: 4KB stack
  - gyro_task: 4KB stack
  - led_task: 2KB stack
  - audio_task: 4KB stack
  - **Total**: ~22KB

- **Core 1 Tasks**: Stack allocated from internal DRAM
  - timer_task: 4KB stack
  - network_task: 12KB stack (TLS overhead)
  - stats_task: 4KB stack
  - **Total**: ~20KB

- **Heap**: Remaining SRAM (~478KB) + PSRAM (8MB)

## Consequences

### Positive

- **Responsive UI**: UI never freezes during network operations
- **Better Power Efficiency**: Core 1 can idle when not syncing (~30% power savings)
- **Timer Accuracy**: Timer task runs at highest priority on Core 1, unaffected by UI rendering
- **Scalability**: Easy to add new background tasks (e.g., OTA updates) without impacting UI
- **Debugging**: Clear separation makes it easier to isolate issues (UI bugs vs. logic bugs)

### Negative

- **Complexity**: Requires careful use of synchronization primitives (mutexes, queues, semaphores)
- **Race Conditions**: Potential for deadlocks if mutexes not used correctly (mitigated by design reviews)
- **Debugging Difficulty**: Multi-threaded issues harder to reproduce and debug (mitigated by logging)
- **Memory Overhead**: Each task requires stack space (~42KB total for 8 tasks)

### Neutral

- **Development Time**: ~1-2 extra days for synchronization implementation and testing

## Implementation

### Task Creation Pattern

```cpp
// src/main.cpp
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

// Queues
QueueHandle_t touch_event_queue;
QueueHandle_t gyro_event_queue;
QueueHandle_t timer_update_queue;

// Mutexes
SemaphoreHandle_t i2c_mutex;
SemaphoreHandle_t display_mutex;

void setup() {
    // Create queues
    touch_event_queue = xQueueCreate(10, sizeof(TouchEvent));
    gyro_event_queue = xQueueCreate(5, sizeof(GyroEvent));
    timer_update_queue = xQueueCreate(10, sizeof(TimerUpdate));

    // Create mutexes
    i2c_mutex = xSemaphoreCreateMutex();
    display_mutex = xSemaphoreCreateMutex();

    // Create Core 0 tasks (UI/Input)
    xTaskCreatePinnedToCore(
        ui_task_func,        // Task function
        "ui_task",           // Name
        8192,                // Stack size (bytes)
        NULL,                // Parameters
        2,                   // Priority
        NULL,                // Task handle
        0                    // Core 0
    );

    xTaskCreatePinnedToCore(touch_task_func, "touch_task", 4096, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(gyro_task_func, "gyro_task", 4096, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(led_task_func, "led_task", 2048, NULL, 1, NULL, 0);

    // Create Core 1 tasks (Logic/Network)
    xTaskCreatePinnedToCore(timer_task_func, "timer_task", 4096, NULL, 4, NULL, 1);
    xTaskCreatePinnedToCore(network_task_func, "network_task", 12288, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(stats_task_func, "stats_task", 4096, NULL, 1, NULL, 1);
}

void loop() {
    // Main loop runs on Core 1 (Arduino framework default)
    // Keep empty, all work done in tasks
    vTaskDelay(pdMS_TO_TICKS(1000));
}
```

### Mutex Usage Pattern

```cpp
// Safe I2C access
void GyroController::readAcceleration(float* ax, float* ay, float* az) {
    if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        M5.Imu.getAccel(ax, ay, az);
        xSemaphoreGive(i2c_mutex);
    } else {
        LOG_ERROR("I2C mutex timeout");
    }
}

// Safe display access
void UIManager::updateTimerDisplay(uint16_t remainingSeconds) {
    if (xSemaphoreTake(display_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        timerDisplay.render(160, 80, remainingSeconds);
        xSemaphoreGive(display_mutex);
    }
}
```

### Queue Usage Pattern

```cpp
// Gyro task sends events to timer task
void gyro_task_func(void* param) {
    while (1) {
        GyroEvent event = detectGesture();

        if (event.type != GyroEventType::NONE) {
            // Send to queue (non-blocking)
            if (xQueueSend(gyro_event_queue, &event, 0) != pdTRUE) {
                LOG_WARN("Gyro event queue full, dropping event");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));  // 50Hz sampling
    }
}

// Timer task receives events from gyro queue
void timer_task_func(void* param) {
    while (1) {
        GyroEvent event;

        // Wait for event (blocking, 1 second timeout)
        if (xQueueReceive(gyro_event_queue, &event, pdMS_TO_TICKS(1000)) == pdTRUE) {
            handleGyroEvent(event);
        }

        // Update timer (runs every second regardless)
        updateTimer();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

## Testing Strategy

### Deadlock Prevention

1. **Lock Ordering**: Always acquire mutexes in the same order
   ```cpp
   // ✅ Good: Always acquire i2c_mutex before display_mutex
   xSemaphoreTake(i2c_mutex, portMAX_DELAY);
   xSemaphoreTake(display_mutex, portMAX_DELAY);
   // ... work ...
   xSemaphoreGive(display_mutex);
   xSemaphoreGive(i2c_mutex);

   // ❌ Bad: Different order can cause deadlock
   xSemaphoreTake(display_mutex, portMAX_DELAY);
   xSemaphoreTake(i2c_mutex, portMAX_DELAY);
   ```

2. **Timeout Usage**: Always use timeout, never `portMAX_DELAY` in production
3. **Lock Duration**: Minimize critical section size

### Race Condition Detection

```cpp
// Enable FreeRTOS assertions
#define configASSERT(x) if (!(x)) { Serial.printf("ASSERT: %s:%d\n", __FILE__, __LINE__); abort(); }

// Enable task watchdog
esp_task_wdt_add(NULL);  // Add current task to watchdog
esp_task_wdt_reset();    // Reset watchdog (call periodically)
```

### Performance Monitoring

```cpp
void printTaskStats() {
    char statsBuffer[512];
    vTaskGetRunTimeStats(statsBuffer);
    Serial.println("Task Statistics:");
    Serial.println(statsBuffer);
}

// Output example:
// Task          Runtime    %
// ui_task       12345     25%
// timer_task    5678      12%
// network_task  2345       5%
// IDLE0         10000     20%
// IDLE1         15000     30%
```

## Alternatives Considered (Revisited)

### Why Not Single-Core?

Single-core would simplify development but sacrifice responsiveness. Network operations (MQTT sync) can take 5-10 seconds, during which UI would freeze. This violates our "responsive UI" design goal.

### Why Not Work-Stealing?

Work-stealing scheduler (like Go's goroutines) would provide automatic load balancing, but:
- Adds significant runtime overhead (~10-15% CPU)
- Unpredictable task placement makes debugging hard
- Overkill for 8 tasks with clear affinity requirements

### Why Explicit Core Pinning?

FreeRTOS default scheduler allows tasks to run on any core. We explicitly pin tasks because:
- **Predictability**: UI tasks always on Core 0, timer always on Core 1
- **Cache Efficiency**: Task data stays in local cache (L1)
- **WiFi Stability**: ESP-IDF WiFi stack expects to run on Core 0

## Future Enhancements

### Dynamic Task Priority Adjustment

```cpp
// Boost network task priority during sync
vTaskPrioritySet(network_task_handle, 3);  // Boost to priority 3
performCloudSync();
vTaskPrioritySet(network_task_handle, 2);  // Restore to priority 2
```

### Task Suspension During Sleep

```cpp
// Suspend non-critical tasks before light sleep
vTaskSuspend(network_task_handle);
vTaskSuspend(stats_task_handle);
esp_light_sleep_start();
// Resume after wake
vTaskResume(network_task_handle);
vTaskResume(stats_task_handle);
```

## References

- FreeRTOS Documentation: https://www.freertos.org/Documentation/
- ESP-IDF Programming Guide (FreeRTOS): https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html
- ESP32 Dual-Core Guide: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/freertos-smp.html

## Related ADRs

- ADR-003: FreeRTOS Synchronization Primitives
- ADR-004: Gyro Polling Strategy (impacts gyro_task on Core 0)
- ADR-005: Light Sleep During Timer (task suspension strategy)

## Revision History

- 2025-12-15: Initial version, decision to use dual-core with explicit pinning
