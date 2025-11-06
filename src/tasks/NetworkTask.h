#ifndef NETWORK_TASK_H
#define NETWORK_TASK_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/**
 * Network Task function for Core 1 (APP_CPU)
 *
 * This task runs on Core 1 and handles all network operations:
 * - WiFi connection management
 * - MQTT client (AWS IoT)
 * - NTP time synchronization
 * - Device Shadow sync
 *
 * Running network operations on Core 1 prevents blocking the UI on Core 0.
 * WiFi.connect() can take 2-10 seconds, MQTT operations can block indefinitely.
 * By running on a separate core, UI remains responsive during network operations.
 *
 * **Current Status**: Skeleton implementation (Phase 2)
 * **Phase 3 TODO**: Implement WiFiManager, MQTTClient, ShadowSync
 *
 * Usage:
 *   xTaskCreatePinnedToCore(
 *       networkTask,     // Task function
 *       "network_task",  // Task name
 *       10240,           // Stack size (10KB for TLS)
 *       NULL,            // Parameters
 *       1,               // Priority (same as UI)
 *       &g_networkTaskHandle, // Task handle (for monitoring)
 *       1                // Core 1 (APP_CPU)
 *   );
 *
 * Synchronization:
 * - Sends updates via g_networkStatusQueue (Core 1 → Core 0)
 * - Receives state changes via g_shadowPublishQueue (Core 0 → Core 1)
 * - Updates g_networkEvents event group (WiFi/MQTT/NTP status bits)
 *
 * @param parameter Task parameter (not used, pass NULL)
 */
void networkTask(void* parameter);

#endif // NETWORK_TASK_H
