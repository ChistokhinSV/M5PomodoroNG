#include "NetworkTask.h"
#include "../core/SyncPrimitives.h"
#include "../core/NetworkConfig.h"
#include "../network/SessionEvent.h"
#include "../network/WebhookDispatcher.h"
#include <Arduino.h>
#include <WiFi.h>
#include <string.h>

/**
 * Network Task (Core 1)
 *
 * Drains g_sessionEventQueue. When the first event arrives, brings WiFi up,
 * dispatches the event to every configured webhook (filtered by event mask),
 * then drains any additional events that piled up during the connect before
 * tearing WiFi down again.
 *
 * On-demand WiFi for now. The dispatcher is stateless so when the upcoming
 * MQTT shadow PR introduces a persistent-WiFi mode (needed for subscribe),
 * we just skip the connect/disconnect cycle and call dispatch() on the
 * persistent connection.
 *
 * Architecture decoupling: producers (TimerStateMachine) don't know about
 * webhooks or MQTT — they push SessionEventMessage to a shared queue and
 * this task fans out to whichever subscribers exist.
 */

// Externs supplied by main.cpp
extern NetworkConfig* g_networkConfig;

TaskHandle_t g_networkTaskHandle = NULL;

namespace {

void sendStatus(NetworkStatus::Event ev, int16_t rssi, const char* msg) {
    if (!g_networkStatusQueue) return;
    NetworkStatus s{};
    s.event = ev;
    s.timestamp = millis();
    s.rssi = rssi;
    if (msg) {
        strncpy(s.message, msg, sizeof(s.message) - 1);
        s.message[sizeof(s.message) - 1] = '\0';
    }
    xQueueSend(g_networkStatusQueue, &s, 0);  // best-effort
}

// Connect using the credentials from network.ini. Block up to 15s.
bool connectWiFi() {
    if (!g_networkConfig || !g_networkConfig->isValid()) {
        Serial.println("[NetworkTask] No network config; cannot connect WiFi");
        return false;
    }
    const auto& wifi_cfg = g_networkConfig->getWiFi();
    if (wifi_cfg.ssid[0] == '\0') {
        Serial.println("[NetworkTask] WiFi SSID empty; nothing to do");
        return false;
    }
    if (WiFi.status() == WL_CONNECTED) return true;  // already up

    Serial.printf("[NetworkTask] WiFi connecting to %s ...\n", wifi_cfg.ssid);
    sendStatus(NetworkStatus::Event::WIFI_CONNECTING, 0, wifi_cfg.ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_cfg.ssid, wifi_cfg.password);

    const uint32_t timeout_ms = 15000;
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeout_ms) {
        vTaskDelay(pdMS_TO_TICKS(250));
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.printf("[NetworkTask] WiFi connect FAILED after %lu ms\n",
                      (unsigned long)(millis() - start));
        sendStatus(NetworkStatus::Event::WIFI_DISCONNECTED, 0, "connect timeout");
        WiFi.disconnect(true);
        return false;
    }

    int16_t rssi = WiFi.RSSI();
    Serial.printf("[NetworkTask] WiFi connected (%lu ms, RSSI %d, IP %s)\n",
                  (unsigned long)(millis() - start), rssi,
                  WiFi.localIP().toString().c_str());
    sendStatus(NetworkStatus::Event::WIFI_CONNECTED, rssi,
               WiFi.localIP().toString().c_str());
    return true;
}

void disconnectWiFi() {
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.disconnect(true);
        Serial.println("[NetworkTask] WiFi disconnected");
        sendStatus(NetworkStatus::Event::WIFI_DISCONNECTED, 0, "ondemand off");
    }
}

}  // namespace

void networkTask(void* parameter) {
    Serial.println("[NetworkTask] Starting on Core 1");
    Serial.printf("[NetworkTask] Stack high-water at entry: %d bytes\n",
                  uxTaskGetStackHighWaterMark(NULL) * 4);
    g_networkTaskHandle = xTaskGetCurrentTaskHandle();

    // Dispatcher is constructed once and held for the task lifetime. It owns
    // no sockets; everything is per-call. Pointer to MQTT.ClientID is stored
    // for the JSON "device" field — that buffer lives in NetworkConfig.
    const char* device_id = (g_networkConfig && g_networkConfig->isValid())
                            ? g_networkConfig->getMQTT().client_id
                            : "m5-pomodoro";
    WebhookDispatcher dispatcher(device_id);

    // No webhooks configured? Park the task with periodic heartbeats so it
    // stays observable but doesn't burn cycles. Avoids consuming the queue
    // (events stay buffered in case config gets added later).
    bool has_webhooks = (g_networkConfig && g_networkConfig->isValid() &&
                        g_networkConfig->getWebhooks().count > 0);
    if (!has_webhooks) {
        Serial.println("[NetworkTask] No webhooks configured; idle loop");
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(60000));
        }
    }

    Serial.printf("[NetworkTask] %u webhook(s) configured\n",
                  g_networkConfig->getWebhooks().count);

    SessionEventMessage ev;
    while (true) {
        // Block waiting for the first event of a burst.
        if (xQueueReceive(g_sessionEventQueue, &ev, portMAX_DELAY) != pdTRUE) continue;

        if (!connectWiFi()) {
            // Couldn't connect — drop this event so the queue doesn't back up
            // forever when WiFi is unreachable. User can see the failure in logs.
            Serial.printf("[NetworkTask] Dropping %s event (no WiFi)\n",
                          sessionEventName(ev.type));
            continue;
        }

        // Fire this event, then drain any others that piled up during the
        // connect. Short 2s timeout — if no more events arrive in 2s, the
        // burst is over and we tear WiFi down.
        dispatcher.dispatch(g_networkConfig->getWebhooks(), ev);
        while (xQueueReceive(g_sessionEventQueue, &ev, pdMS_TO_TICKS(2000)) == pdTRUE) {
            dispatcher.dispatch(g_networkConfig->getWebhooks(), ev);
        }

        disconnectWiFi();
    }
}
