#include "SyncPrimitives.h"
#include <Arduino.h>

// ============================================================================
// Global Queue Handles
// ============================================================================

QueueHandle_t g_shadowPublishQueue = NULL;
QueueHandle_t g_networkStatusQueue = NULL;

// ============================================================================
// Global Mutex Handles
// ============================================================================

SemaphoreHandle_t g_i2c_mutex = NULL;
SemaphoreHandle_t g_display_mutex = NULL;
SemaphoreHandle_t g_stats_mutex = NULL;
SemaphoreHandle_t g_shadow_state_mutex = NULL;

// ============================================================================
// Global Event Group Handles
// ============================================================================

EventGroupHandle_t g_networkEvents = NULL;

// ============================================================================
// Initialization Status
// ============================================================================

static bool s_initialized = false;

// ============================================================================
// Implementation
// ============================================================================

bool initSyncPrimitives() {
    Serial.println("[SyncPrimitives] Initializing FreeRTOS synchronization objects...");

    // Create queues
    g_shadowPublishQueue = xQueueCreate(10, sizeof(ShadowUpdate));
    if (g_shadowPublishQueue == NULL) {
        Serial.println("[SyncPrimitives] ERROR: Failed to create shadowPublishQueue");
        return false;
    }
    Serial.println("[SyncPrimitives] ✓ shadowPublishQueue created (10 items, Core 0 → Core 1)");

    g_networkStatusQueue = xQueueCreate(5, sizeof(NetworkStatus));
    if (g_networkStatusQueue == NULL) {
        Serial.println("[SyncPrimitives] ERROR: Failed to create networkStatusQueue");
        cleanupSyncPrimitives();
        return false;
    }
    Serial.println("[SyncPrimitives] ✓ networkStatusQueue created (5 items, Core 1 → Core 0)");

    // Create mutexes
    g_i2c_mutex = xSemaphoreCreateMutex();
    if (g_i2c_mutex == NULL) {
        Serial.println("[SyncPrimitives] ERROR: Failed to create i2c_mutex");
        cleanupSyncPrimitives();
        return false;
    }
    Serial.println("[SyncPrimitives] ✓ i2c_mutex created (protects I2C bus: gyro + HMI)");

    g_display_mutex = xSemaphoreCreateMutex();
    if (g_display_mutex == NULL) {
        Serial.println("[SyncPrimitives] ERROR: Failed to create display_mutex");
        cleanupSyncPrimitives();
        return false;
    }
    Serial.println("[SyncPrimitives] ✓ display_mutex created (protects M5.Display SPI)");

    g_stats_mutex = xSemaphoreCreateMutex();
    if (g_stats_mutex == NULL) {
        Serial.println("[SyncPrimitives] ERROR: Failed to create stats_mutex");
        cleanupSyncPrimitives();
        return false;
    }
    Serial.println("[SyncPrimitives] ✓ stats_mutex created (protects Statistics NVS)");

    g_shadow_state_mutex = xSemaphoreCreateMutex();
    if (g_shadow_state_mutex == NULL) {
        Serial.println("[SyncPrimitives] ERROR: Failed to create shadow_state_mutex");
        cleanupSyncPrimitives();
        return false;
    }
    Serial.println("[SyncPrimitives] ✓ shadow_state_mutex created (protects Shadow document)");

    // Create event groups
    g_networkEvents = xEventGroupCreate();
    if (g_networkEvents == NULL) {
        Serial.println("[SyncPrimitives] ERROR: Failed to create networkEvents");
        cleanupSyncPrimitives();
        return false;
    }
    Serial.println("[SyncPrimitives] ✓ networkEvents created (WiFi/MQTT/NTP status bits)");

    // Clear all network status bits (start disconnected)
    xEventGroupClearBits(g_networkEvents,
                         NETWORK_WIFI_CONNECTED |
                         NETWORK_MQTT_CONNECTED |
                         NETWORK_NTP_SYNCED |
                         NETWORK_SYNCING);

    s_initialized = true;
    Serial.println("[SyncPrimitives] ✓ All synchronization objects initialized successfully");
    Serial.println("[SyncPrimitives] Lock ordering: i2c_mutex → display_mutex → stats_mutex → shadow_state_mutex");
    Serial.println("[SyncPrimitives] Always use timeouts (50-100ms), never portMAX_DELAY");

    return true;
}

void cleanupSyncPrimitives() {
    Serial.println("[SyncPrimitives] Cleaning up synchronization objects...");

    // Delete queues
    if (g_shadowPublishQueue != NULL) {
        vQueueDelete(g_shadowPublishQueue);
        g_shadowPublishQueue = NULL;
        Serial.println("[SyncPrimitives] shadowPublishQueue deleted");
    }

    if (g_networkStatusQueue != NULL) {
        vQueueDelete(g_networkStatusQueue);
        g_networkStatusQueue = NULL;
        Serial.println("[SyncPrimitives] networkStatusQueue deleted");
    }

    // Delete mutexes
    if (g_i2c_mutex != NULL) {
        vSemaphoreDelete(g_i2c_mutex);
        g_i2c_mutex = NULL;
        Serial.println("[SyncPrimitives] i2c_mutex deleted");
    }

    if (g_display_mutex != NULL) {
        vSemaphoreDelete(g_display_mutex);
        g_display_mutex = NULL;
        Serial.println("[SyncPrimitives] display_mutex deleted");
    }

    if (g_stats_mutex != NULL) {
        vSemaphoreDelete(g_stats_mutex);
        g_stats_mutex = NULL;
        Serial.println("[SyncPrimitives] stats_mutex deleted");
    }

    if (g_shadow_state_mutex != NULL) {
        vSemaphoreDelete(g_shadow_state_mutex);
        g_shadow_state_mutex = NULL;
        Serial.println("[SyncPrimitives] shadow_state_mutex deleted");
    }

    // Delete event groups
    if (g_networkEvents != NULL) {
        vEventGroupDelete(g_networkEvents);
        g_networkEvents = NULL;
        Serial.println("[SyncPrimitives] networkEvents deleted");
    }

    s_initialized = false;
    Serial.println("[SyncPrimitives] Cleanup complete");
}

bool isSyncInitialized() {
    return s_initialized;
}

void printSyncStatus() {
    if (!s_initialized) {
        Serial.println("[SyncPrimitives] Not initialized");
        return;
    }

    Serial.println("\n========== Synchronization Status ==========");

    // Queue status
    Serial.printf("shadowPublishQueue: %d/%d items pending\n",
                  uxQueueMessagesWaiting(g_shadowPublishQueue),
                  uxQueueSpacesAvailable(g_shadowPublishQueue) + uxQueueMessagesWaiting(g_shadowPublishQueue));

    Serial.printf("networkStatusQueue: %d/%d items pending\n",
                  uxQueueMessagesWaiting(g_networkStatusQueue),
                  uxQueueSpacesAvailable(g_networkStatusQueue) + uxQueueMessagesWaiting(g_networkStatusQueue));

    // Mutex status (can't easily check holder from FreeRTOS API)
    Serial.println("Mutexes: i2c_mutex, display_mutex, stats_mutex, shadow_state_mutex");
    Serial.println("  (Use timeout logs to detect deadlocks)");

    // Event group status
    EventBits_t bits = xEventGroupGetBits(g_networkEvents);
    Serial.printf("Network status bits: 0x%02X\n", bits);
    Serial.printf("  WIFI_CONNECTED:  %s\n", (bits & NETWORK_WIFI_CONNECTED) ? "YES" : "NO");
    Serial.printf("  MQTT_CONNECTED:  %s\n", (bits & NETWORK_MQTT_CONNECTED) ? "YES" : "NO");
    Serial.printf("  NTP_SYNCED:      %s\n", (bits & NETWORK_NTP_SYNCED) ? "YES" : "NO");
    Serial.printf("  SYNCING:         %s\n", (bits & NETWORK_SYNCING) ? "YES" : "NO");

    Serial.println("============================================\n");
}

void logMutexTimeout(const char* mutex_name, const char* task_name, uint32_t timeout_ms) {
    Serial.printf("[SyncPrimitives] ERROR: Mutex timeout!\n");
    Serial.printf("  Mutex: %s\n", mutex_name);
    Serial.printf("  Task: %s\n", task_name);
    Serial.printf("  Timeout: %lu ms\n", timeout_ms);
    Serial.println("  This indicates a potential DEADLOCK or long-held lock");
    Serial.println("  Check lock ordering: i2c → display → stats → shadow_state");

    // Print current sync status for debugging
    printSyncStatus();
}
