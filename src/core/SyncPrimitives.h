#ifndef SYNC_PRIMITIVES_H
#define SYNC_PRIMITIVES_H

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>
#include <stdint.h>

/**
 * FreeRTOS Synchronization Primitives for Multi-core Architecture
 *
 * This module provides global synchronization objects for dual-core operation:
 * - Core 0 (PRO_CPU): UI, input, state machine, audio, LEDs
 * - Core 1 (APP_CPU): Network (WiFi, MQTT, NTP)
 *
 * Design Principles:
 * - Lock ordering: i2c_mutex → display_mutex → stats_mutex → shadow_state_mutex
 * - Always use timeouts (never portMAX_DELAY)
 * - Log errors on timeout (indicates potential deadlock)
 * - Use RAII guards (MutexGuard) in C++ code
 *
 * Inter-core Communication:
 * - Core 0 → Core 1: shadowPublishQueue (state changes to publish)
 * - Core 1 → Core 0: networkStatusQueue (WiFi/MQTT status updates)
 * - Network status bits: networkEvents (WIFI_CONNECTED, MQTT_CONNECTED, NTP_SYNCED)
 */

// ============================================================================
// Queue Data Structures
// ============================================================================

/**
 * Shadow update message (Core 0 → Core 1)
 * Sent when timer state changes and needs to be published to AWS IoT Shadow
 */
struct ShadowUpdate {
    enum class Type {
        TIMER_STATE,      // Timer state changed (IDLE/ACTIVE/PAUSED)
        SESSION_COMPLETE, // Work/break session completed
        STATS_UPDATED     // Daily statistics updated
    };

    Type type;
    uint32_t timestamp;    // Unix timestamp when event occurred
    uint8_t state;         // TimerStateMachine::State (0=IDLE, 1=ACTIVE, 2=PAUSED)
    uint8_t session_type;  // PomodoroSequence::SessionType (0=WORK, 1=SHORT_BREAK, 2=LONG_BREAK)
    uint8_t completed;     // Number of completed work sessions today
    uint16_t remaining_sec; // Remaining seconds in current session (if ACTIVE)
};

/**
 * Network status message (Core 1 → Core 0)
 * Sent when network connectivity changes, UI updates status bar
 */
struct NetworkStatus {
    enum class Event {
        WIFI_CONNECTING,   // WiFi connection attempt started
        WIFI_CONNECTED,    // WiFi successfully connected
        WIFI_DISCONNECTED, // WiFi connection lost
        MQTT_CONNECTING,   // MQTT connection attempt started
        MQTT_CONNECTED,    // MQTT successfully connected
        MQTT_DISCONNECTED, // MQTT connection lost
        NTP_SYNCED,        // NTP time synchronized
        SYNC_ERROR         // Network error occurred
    };

    Event event;
    uint32_t timestamp;    // When event occurred
    int16_t rssi;          // WiFi signal strength (dBm, -127 to 0)
    char message[32];      // Optional error message or IP address
};

// ============================================================================
// Global Queues
// ============================================================================

// Core 0 → Core 1: State changes to publish to cloud
extern QueueHandle_t g_shadowPublishQueue;

// Core 1 → Core 0: Network status updates for UI
extern QueueHandle_t g_networkStatusQueue;

// ============================================================================
// Global Mutexes
// ============================================================================

// I2C bus protection (MPU6886 gyro + HMI encoder share I2C0)
// Used by: GyroController, HMIController
// Timeout: 100ms (I2C operations are fast)
extern SemaphoreHandle_t g_i2c_mutex;

// Display access protection (M5.Display SPI operations)
// Used by: Renderer, ScreenManager
// Timeout: 50ms (display operations are very fast)
extern SemaphoreHandle_t g_display_mutex;

// Statistics data protection (NVS read/write)
// Used by: TimerStateMachine (write), ShadowSync (read)
// Timeout: 100ms (NVS can be slower)
extern SemaphoreHandle_t g_stats_mutex;

// Shadow state protection (AWS IoT Shadow document)
// Used by: Core 0 (update local state), Core 1 (read for publish)
// Timeout: 50ms (memory operations are fast)
extern SemaphoreHandle_t g_shadow_state_mutex;

// ============================================================================
// Global Event Groups
// ============================================================================

// Network connectivity status bits (for UI status bar)
extern EventGroupHandle_t g_networkEvents;

// Event bits for g_networkEvents
#define NETWORK_WIFI_CONNECTED   (1 << 0)  // WiFi connected
#define NETWORK_MQTT_CONNECTED   (1 << 1)  // MQTT connected to AWS IoT
#define NETWORK_NTP_SYNCED       (1 << 2)  // NTP time synchronized
#define NETWORK_SYNCING          (1 << 3)  // Cloud sync in progress

// ============================================================================
// Initialization & Cleanup
// ============================================================================

/**
 * Initialize all synchronization primitives
 *
 * Must be called from setup() BEFORE creating FreeRTOS tasks.
 *
 * Creates:
 * - 2 queues (10 items for shadow, 5 items for network status)
 * - 4 mutexes (I2C, display, stats, shadow state)
 * - 1 event group (network status bits)
 *
 * @return true if all objects created successfully, false on failure
 */
bool initSyncPrimitives();

/**
 * Clean up all synchronization primitives
 *
 * Deletes all queues, mutexes, and event groups.
 * Call before system shutdown (not typically needed on embedded systems).
 */
void cleanupSyncPrimitives();

/**
 * Check if synchronization primitives are initialized
 *
 * @return true if initSyncPrimitives() was called successfully
 */
bool isSyncInitialized();

// ============================================================================
// Debug & Monitoring
// ============================================================================

/**
 * Print synchronization object status (for debugging)
 *
 * Outputs to Serial:
 * - Queue fill levels (how many items pending)
 * - Mutex holders (which task currently holds each mutex, if any)
 * - Event group status bits
 */
void printSyncStatus();

/**
 * Log timeout error with context
 *
 * Called when mutex acquisition times out, indicates potential deadlock.
 * Logs to Serial with ERROR level.
 *
 * @param mutex_name Name of mutex that timed out
 * @param task_name Name of task that failed to acquire
 * @param timeout_ms Timeout value used (ms)
 */
void logMutexTimeout(const char* mutex_name, const char* task_name, uint32_t timeout_ms);

#endif // SYNC_PRIMITIVES_H
