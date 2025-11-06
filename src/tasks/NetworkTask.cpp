#include "NetworkTask.h"
#include <Arduino.h>
#include "../core/SyncPrimitives.h"

/**
 * Network Task (Core 1 - Application CPU)
 *
 * **Phase 2 (Current)**: Skeleton implementation - just logs heartbeat
 * **Phase 3 (Future)**: Will implement:
 *   - WiFiManager: Connection management, reconnection logic
 *   - MQTTClient: AWS IoT MQTT over TLS, subscription handling
 *   - NTPClient: Time synchronization, drift correction
 *   - ShadowSync: Device Shadow document sync, queue-based publishing
 *
 * This task demonstrates that:
 * 1. Core 1 is active and running independently
 * 2. FreeRTOS task creation works correctly
 * 3. Multi-core architecture is functional
 * 4. UI on Core 0 remains responsive (network doesn't block)
 *
 * Test Approach:
 * - Check serial output shows both Core 0 and Core 1 messages
 * - Verify UI remains responsive (touch works while network task runs)
 * - Monitor task stats (vTaskGetRunTimeStats) for core distribution
 */

// Task handle (for monitoring and debugging)
TaskHandle_t g_networkTaskHandle = NULL;

void networkTask(void* parameter) {
    Serial.println("[NetworkTask] Starting on Core 1...");
    Serial.printf("[NetworkTask] Task handle: 0x%08X\n", (uint32_t)xTaskGetCurrentTaskHandle());
    Serial.printf("[NetworkTask] Priority: %d\n", uxTaskPriorityGet(NULL));
    Serial.printf("[NetworkTask] Stack size: %d bytes\n", uxTaskGetStackHighWaterMark(NULL) * 4);
    Serial.println("[NetworkTask] Skeleton implementation (Phase 2)");
    Serial.println("[NetworkTask] Phase 3 TODO: WiFi, MQTT, NTP, Shadow sync");

    // Save task handle for monitoring
    g_networkTaskHandle = xTaskGetCurrentTaskHandle();

    uint32_t heartbeat_counter = 0;
    uint32_t last_heartbeat = millis();

    while (true) {
        // Heartbeat every 10 seconds (proves Core 1 is running)
        uint32_t now = millis();
        if (now - last_heartbeat >= 10000) {
            last_heartbeat = now;
            heartbeat_counter++;

            Serial.printf("[NetworkTask] Heartbeat #%lu (Core 1 alive)\n", heartbeat_counter);
            Serial.printf("[NetworkTask] Free stack: %d bytes\n", uxTaskGetStackHighWaterMark(NULL) * 4);

            // Test: Send fake network status update to Core 0
            // In Phase 3, this will be real WiFi/MQTT status
            NetworkStatus status;
            status.event = NetworkStatus::Event::WIFI_DISCONNECTED;
            status.timestamp = now;
            status.rssi = 0;
            strncpy(status.message, "Phase 2: No network", sizeof(status.message) - 1);
            status.message[sizeof(status.message) - 1] = '\0';

            // Send to Core 0 (non-blocking, drop if queue full)
            if (xQueueSend(g_networkStatusQueue, &status, 0) != pdTRUE) {
                Serial.println("[NetworkTask] WARNING: networkStatusQueue full, message dropped");
            }
        }

        // Check for shadow publish requests from Core 0
        // In Phase 3, this will trigger MQTT publish to AWS IoT
        ShadowUpdate update;
        if (xQueueReceive(g_shadowPublishQueue, &update, 0) == pdTRUE) {
            Serial.printf("[NetworkTask] Received shadow update from Core 0: type=%d\n", (int)update.type);
            Serial.println("[NetworkTask] Phase 3 TODO: Publish to AWS IoT Shadow");
            // TODO Phase 3: Publish to AWS IoT Device Shadow via MQTT
        }

        // Sleep for 1 second (network task doesn't need to run frequently)
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
