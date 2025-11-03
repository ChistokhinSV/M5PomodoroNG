# ADR-003: FreeRTOS Synchronization Primitives Strategy

**Status**: Accepted

**Date**: 2025-12-15

**Decision Makers**: Development Team

## Context

With a dual-core FreeRTOS architecture (ADR-002), we need to establish clear rules for inter-task communication and shared resource protection to prevent race conditions, deadlocks, and priority inversion.

FreeRTOS provides multiple synchronization primitives:
- **Mutexes**: Mutual exclusion for shared resources
- **Semaphores**: Binary (0/1) and counting (0-N) for signaling
- **Queues**: FIFO message passing between tasks
- **Event Groups**: Synchronization of multiple events
- **Stream Buffers**: Byte streams between tasks
- **Direct Task Notifications**: Lightweight signaling (ESP-IDF specific)

We need to decide which primitives to use for each use case and establish patterns to avoid common pitfalls.

### Options Considered

1. **Global Variables with Volatile**
   - Pros: Simplest, no overhead
   - Cons: Race conditions, no atomicity guarantees, unsafe

2. **Mutexes for All Shared Data**
   - Pros: Safe, straightforward
   - Cons: Potential deadlocks, priority inversion, overhead

3. **Lockless Data Structures (Atomic Operations)**
   - Pros: No blocking, high performance
   - Cons: Complex, limited use cases, hard to debug

4. **Hybrid Approach** (Mutexes + Queues + Event Groups)
   - Pros: Right tool for each job, FreeRTOS best practice
   - Cons: More primitives to learn, requires design discipline

## Decision

**We will use a hybrid approach with strict usage guidelines for each primitive type:**

| Primitive | Use Case | Example |
|-----------|----------|---------|
| **Mutex** | Protecting shared resources (I2C, display, data structures) | `i2c_mutex`, `display_mutex`, `stats_mutex` |
| **Queue** | Passing events/data between tasks | `touch_event_queue`, `gyro_event_queue` |
| **Event Group** | Synchronizing multiple tasks on events | `timer_events`, `network_events` |
| **Direct Notifications** | Simple task wake-up (one sender, one receiver) | Wake network task from timer task |
| **Semaphores** | Binary semaphores for resource availability | Audio buffer ready |

**NOT USED**: Stream buffers (no streaming data), counting semaphores (no resource pools)

### Rationale

1. **Safety**: Mutexes prevent race conditions on shared data
2. **Performance**: Queues provide efficient message passing without polling
3. **Clarity**: Each primitive has a specific purpose, making code intent clear
4. **FreeRTOS Best Practice**: Follows official FreeRTOS design patterns
5. **Priority Inheritance**: FreeRTOS mutexes support priority inheritance (prevents priority inversion)

## Implementation Guidelines

### 1. Mutexes (Shared Resource Protection)

**When to Use**: Protecting access to shared hardware (I2C, SPI, display) or data structures.

**Pattern**:
```cpp
// Global mutex declaration
SemaphoreHandle_t i2c_mutex;

// Initialization (in setup())
i2c_mutex = xSemaphoreCreateMutex();
if (i2c_mutex == NULL) {
    LOG_ERROR("Failed to create I2C mutex");
}

// Usage with timeout
void GyroController::readAccel(float* ax, float* ay, float* az) {
    if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Critical section: Only one task can execute this at a time
        M5.Imu.getAccel(ax, ay, az);
        xSemaphoreGive(i2c_mutex);
    } else {
        LOG_ERROR("I2C mutex timeout - potential deadlock!");
        *ax = *ay = *az = 0.0f;  // Return safe defaults
    }
}
```

**Rules**:
- Always use timeout (never `portMAX_DELAY` in production)
- Always release mutex before returning
- Minimize critical section duration
- Use RAII-style guards (C++ only):

```cpp
class MutexGuard {
public:
    MutexGuard(SemaphoreHandle_t mutex, TickType_t timeout)
        : mutex_(mutex), acquired_(false) {
        acquired_ = (xSemaphoreTake(mutex_, timeout) == pdTRUE);
    }

    ~MutexGuard() {
        if (acquired_) {
            xSemaphoreGive(mutex_);
        }
    }

    bool isAcquired() const { return acquired_; }

private:
    SemaphoreHandle_t mutex_;
    bool acquired_;
};

// Usage
void safeOperation() {
    MutexGuard guard(i2c_mutex, pdMS_TO_TICKS(100));
    if (guard.isAcquired()) {
        // Work with I2C
    }
    // Mutex automatically released when guard goes out of scope
}
```

### 2. Queues (Event Passing)

**When to Use**: Sending events or data from one task to another (producer-consumer).

**Pattern**:
```cpp
// Event structure
struct TouchEvent {
    uint16_t x;
    uint16_t y;
    TouchEventType type;  // PRESS, RELEASE, DRAG
};

// Global queue declaration
QueueHandle_t touch_event_queue;

// Initialization
touch_event_queue = xQueueCreate(10, sizeof(TouchEvent));  // Max 10 events

// Producer (touch task)
void touch_task_func(void* param) {
    while (1) {
        TouchEvent event = detectTouch();

        if (event.type != TouchEventType::NONE) {
            // Send to queue (non-blocking to avoid lag)
            if (xQueueSend(touch_event_queue, &event, 0) != pdTRUE) {
                LOG_WARN("Touch queue full, event dropped");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));  // 100Hz polling
    }
}

// Consumer (UI task)
void ui_task_func(void* param) {
    while (1) {
        TouchEvent event;

        // Receive from queue (blocking with timeout)
        if (xQueueReceive(touch_event_queue, &event, pdMS_TO_TICKS(50)) == pdTRUE) {
            handleTouchEvent(event);
        }

        // Render UI
        renderUI();
        vTaskDelay(pdMS_TO_TICKS(100));  // 10 FPS
    }
}
```

**Rules**:
- Producer uses `xQueueSend` with zero timeout (non-blocking) to avoid lag
- Consumer uses `xQueueReceive` with timeout to avoid busy-waiting
- Queue size should be 2-3× typical event rate per frame
- Use `xQueueSendFromISR` if sending from interrupt handler

### 3. Event Groups (Multi-Event Synchronization)

**When to Use**: Synchronizing multiple tasks waiting for one or more events.

**Pattern**:
```cpp
// Event bits
#define TIMER_STARTED    (1 << 0)
#define TIMER_PAUSED     (1 << 1)
#define TIMER_EXPIRED    (1 << 2)
#define CLOUD_CONNECTED  (1 << 3)
#define CLOUD_SYNCED     (1 << 4)

// Global event group
EventGroupHandle_t timer_events;

// Initialization
timer_events = xEventGroupCreate();

// Setting events (timer task)
void TimerStateMachine::start() {
    currentState = PomodoroState::POMODORO;
    xEventGroupSetBits(timer_events, TIMER_STARTED);
}

// Waiting for events (network task)
void network_task_func(void* param) {
    while (1) {
        // Wait for timer to start OR expire
        EventBits_t bits = xEventGroupWaitBits(
            timer_events,
            TIMER_STARTED | TIMER_EXPIRED,  // Bits to wait for
            pdTRUE,                          // Clear bits on exit
            pdFALSE,                         // Wait for ANY bit (not all)
            pdMS_TO_TICKS(10000)             // 10s timeout
        );

        if (bits & TIMER_STARTED) {
            publishTimerStart();
        }

        if (bits & TIMER_EXPIRED) {
            publishTimerExpire();
        }
    }
}
```

**Rules**:
- Use for broadcasting events to multiple tasks
- Clear bits after handling (use `pdTRUE` for `xClearOnExit`)
- Max 24 event bits (ESP32 limitation)

### 4. Direct Task Notifications (Lightweight Wake)

**When to Use**: Simple wake-up signal from one task to another (no data payload).

**Pattern**:
```cpp
// Task handle (global)
TaskHandle_t network_task_handle;

// Create task with handle
xTaskCreatePinnedToCore(network_task_func, "network_task", 12288, NULL, 2, &network_task_handle, 1);

// Sender (timer task)
void TimerStateMachine::onExpire() {
    // Wake network task immediately
    xTaskNotifyGive(network_task_handle);
}

// Receiver (network task)
void network_task_func(void* param) {
    while (1) {
        // Wait for notification (blocking)
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(30000));  // 30s timeout

        // Perform cloud sync
        syncToCloud();
    }
}
```

**Rules**:
- Fastest IPC mechanism (lower overhead than queues)
- Use only for simple wake-up (no data payload)
- One sender, one receiver only

### 5. Binary Semaphores (Resource Availability)

**When to Use**: Signaling that a resource is available (e.g., audio buffer ready).

**Pattern**:
```cpp
SemaphoreHandle_t audio_ready_sem;

// Initialization
audio_ready_sem = xSemaphoreCreateBinary();

// Producer (audio callback)
void i2s_dma_callback() {
    // Signal that buffer is ready
    xSemaphoreGiveFromISR(audio_ready_sem, NULL);
}

// Consumer (audio task)
void audio_task_func(void* param) {
    while (1) {
        // Wait for buffer ready
        if (xSemaphoreTake(audio_ready_sem, pdMS_TO_TICKS(1000)) == pdTRUE) {
            processAudioBuffer();
        }
    }
}
```

**Rules**:
- Use binary semaphore for signaling, not resource protection (use mutex instead)
- Initial state: "taken" (not available)

## Deadlock Prevention Strategy

### Lock Ordering

Always acquire mutexes in the same order across all tasks:

```cpp
// Global lock order (documented)
// 1. i2c_mutex
// 2. display_mutex
// 3. stats_mutex

// ✅ Correct: Acquire in order
void safeOperation() {
    xSemaphoreTake(i2c_mutex, timeout);
    xSemaphoreTake(display_mutex, timeout);
    // ... work ...
    xSemaphoreGive(display_mutex);
    xSemaphoreGive(i2c_mutex);
}

// ❌ Incorrect: Out of order
void unsafeOperation() {
    xSemaphoreTake(display_mutex, timeout);
    xSemaphoreTake(i2c_mutex, timeout);  // DEADLOCK RISK!
}
```

### Timeout Discipline

**Never use `portMAX_DELAY` in production code**:

```cpp
// ❌ Bad: Can deadlock forever
xSemaphoreTake(i2c_mutex, portMAX_DELAY);

// ✅ Good: Timeout allows detection
if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    // Work
    xSemaphoreGive(i2c_mutex);
} else {
    LOG_ERROR("Mutex timeout - potential deadlock");
    // Handle gracefully
}
```

### No Nested Locks

Avoid taking multiple mutexes unless absolutely necessary:

```cpp
// ✅ Preferred: One mutex at a time
void operation1() {
    MutexGuard guard(i2c_mutex, timeout);
    // Work with I2C only
}

void operation2() {
    MutexGuard guard(display_mutex, timeout);
    // Work with display only
}

// ⚠️ Acceptable: Nested, but in consistent order
void operation3() {
    MutexGuard guard1(i2c_mutex, timeout);
    if (guard1.isAcquired()) {
        MutexGuard guard2(display_mutex, timeout);
        if (guard2.isAcquired()) {
            // Work with both
        }
    }
}
```

## Performance Considerations

### Mutex Overhead

```
Context switch cost: ~5-10 µs (ESP32 @ 240MHz)
Mutex acquire (uncontended): ~2 µs
Mutex acquire (contended): ~5-20 µs (includes context switch)
```

**Optimization**: Batch operations to minimize lock acquisitions:

```cpp
// ❌ Inefficient: Lock per read
for (int i = 0; i < 100; i++) {
    xSemaphoreTake(stats_mutex, timeout);
    value = stats[i];
    xSemaphoreGive(stats_mutex);
}

// ✅ Efficient: Lock once
xSemaphoreTake(stats_mutex, timeout);
for (int i = 0; i < 100; i++) {
    value = stats[i];
}
xSemaphoreGive(stats_mutex);
```

### Queue Overhead

```
Queue send (empty queue): ~3 µs
Queue receive (full queue): ~3 µs
Queue send (full queue, blocking): ~5-20 µs (includes task switch)
```

**Optimization**: Use direct task notifications for simple wake-up (30% faster than queues).

## Testing Strategy

### Deadlock Detection

```cpp
// Enable watchdog with timeout
esp_task_wdt_init(10, true);  // 10s timeout, panic on timeout
esp_task_wdt_add(NULL);       // Add current task

// In task loop
void task_func(void* param) {
    while (1) {
        esp_task_wdt_reset();  // Reset watchdog
        // ... work ...
        vTaskDelay(...);
    }
}

// If task doesn't reset watchdog within 10s → deadlock detected → panic
```

### Race Condition Detection

```cpp
// Compile with FreeRTOS assertions
#define configASSERT(x) \
    if (!(x)) { \
        Serial.printf("ASSERT failed: %s:%d\n", __FILE__, __LINE__); \
        abort(); \
    }

// Enable stack overflow checking
#define configCHECK_FOR_STACK_OVERFLOW 2
```

### Stress Testing

```cpp
// Hammer mutexes from multiple tasks
void stress_test() {
    for (int i = 0; i < 1000; i++) {
        xSemaphoreTake(i2c_mutex, timeout);
        vTaskDelay(pdMS_TO_TICKS(random(1, 10)));
        xSemaphoreGive(i2c_mutex);
    }
}

// Run on both cores simultaneously
xTaskCreatePinnedToCore(stress_test, "stress0", 4096, NULL, 1, NULL, 0);
xTaskCreatePinnedToCore(stress_test, "stress1", 4096, NULL, 1, NULL, 1);
```

## Consequences

### Positive

- **Safety**: Eliminates race conditions and data corruption
- **Clarity**: Clear ownership and access patterns for shared resources
- **Performance**: Efficient IPC with minimal overhead
- **Debuggability**: FreeRTOS provides runtime statistics and task monitoring

### Negative

- **Complexity**: Developers must understand synchronization primitives
- **Overhead**: Mutexes add 2-10 µs latency per acquisition
- **Potential Deadlocks**: Incorrect usage can cause system hang (mitigated by timeouts and testing)

## References

- FreeRTOS Synchronization: https://www.freertos.org/a00113.html
- ESP-IDF FreeRTOS Guide: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html
- Priority Inheritance: https://www.freertos.org/Real-time-embedded-RTOS-mutexes.html

## Related ADRs

- ADR-002: Dual-Core Architecture (establishes need for synchronization)
- ADR-004: Gyro Polling Strategy (uses i2c_mutex)

## Revision History

- 2025-12-15: Initial version, hybrid synchronization strategy
