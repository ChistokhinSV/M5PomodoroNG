#include "NetworkTask.h"
#include "../core/SyncPrimitives.h"
#include "../core/NetworkConfig.h"
#include "../core/TimerStateMachine.h"
#include "../core/PomodoroSequence.h"
#include "../core/Statistics.h"
#include "../core/TimeManager.h"
#include "../network/SessionEvent.h"
#include "../network/WebhookDispatcher.h"
#include "../network/MQTTClient.h"
#include "../network/ShadowPublisher.h"
#include <Arduino.h>
#include <WiFi.h>
#include <string.h>

/**
 * Network Task (Core 1).
 *
 * Two operating modes selected at boot from network.ini:
 *
 *   Persistent (CloudSync.Enabled=true):
 *     Bring up WiFi, connect MQTT, subscribe to /shadow/update/delta, publish
 *     a snapshot, then forever:
 *       - reconnect WiFi/MQTT with backoff if either drops
 *       - drain g_sessionEventQueue and fan out to webhooks + shadow
 *       - service mqtt.loop() for keepalive and incoming delta callbacks
 *
 *   On-demand (CloudSync.Enabled=false):
 *     Block on g_sessionEventQueue, connect WiFi when an event arrives,
 *     dispatch webhooks, drain burst, disconnect. Battery-friendly; no MQTT.
 *
 * Producers (TimerStateMachine) don't know which mode is active — they just
 * push SessionEventMessage to the shared queue.
 */

extern NetworkConfig*       g_networkConfig;
extern TimerStateMachine*   g_stateMachine;
extern PomodoroSequence*    g_sequence;
extern Statistics*          g_statistics;
extern TimeManager*         g_timeManager;

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
    xQueueSend(g_networkStatusQueue, &s, 0);
}

// Single WiFi connect attempt with a 15s timeout. Used by both modes.
bool connectWiFi() {
    if (!g_networkConfig || !g_networkConfig->isValid()) return false;
    const auto& wifi_cfg = g_networkConfig->getWiFi();
    if (wifi_cfg.ssid[0] == '\0') return false;
    if (WiFi.status() == WL_CONNECTED) return true;

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

// On-demand mode: block on the queue, connect, dispatch, disconnect.
void runOnDemandLoop(WebhookDispatcher& dispatcher) {
    Serial.printf("[NetworkTask] On-demand mode, %u webhook(s) configured\n",
                  g_networkConfig->getWebhooks().count);

    SessionEventMessage ev;
    while (true) {
        if (xQueueReceive(g_sessionEventQueue, &ev, portMAX_DELAY) != pdTRUE) continue;

        if (!connectWiFi()) {
            Serial.printf("[NetworkTask] Dropping %s event (no WiFi)\n",
                          sessionEventName(ev.type));
            continue;
        }

        dispatcher.dispatch(g_networkConfig->getWebhooks(), ev);
        while (xQueueReceive(g_sessionEventQueue, &ev, pdMS_TO_TICKS(2000)) == pdTRUE) {
            dispatcher.dispatch(g_networkConfig->getWebhooks(), ev);
        }
        disconnectWiFi();
    }
}

// Persistent mode: keep WiFi + MQTT up forever, fan out to webhooks + shadow.
void runPersistentLoop(WebhookDispatcher& dispatcher) {
    MQTTClient mqtt(
        g_networkConfig->getMQTT(),
        g_networkConfig->getRootCA(),
        g_networkConfig->getDeviceCert(),
        g_networkConfig->getPrivateKey()
    );
    ShadowPublisher shadow(
        mqtt,
        g_networkConfig->getMQTT().thing_name,
        *g_stateMachine, *g_sequence, g_statistics
    );
    mqtt.onMessage([&shadow](const char* topic, const char* payload, size_t len) {
        shadow.handleMqttMessage(topic, payload, len);
    });
    mqtt.begin();

    bool webhooks_present = g_networkConfig->getWebhooks().count > 0;
    Serial.printf("[NetworkTask] Persistent mode: MQTT enabled, %u webhook(s) also configured\n",
                  g_networkConfig->getWebhooks().count);

    uint32_t next_wifi_retry = 0;
    uint32_t next_mqtt_retry = 0;
    uint32_t wifi_backoff_ms = 15000;
    bool     mqtt_was_connected = false;
    bool     ntp_synced_once = false;

    while (true) {
        uint32_t now = millis();

        // WiFi maintenance with exponential backoff capped at 60s.
        if (WiFi.status() != WL_CONNECTED) {
            if (now >= next_wifi_retry) {
                if (connectWiFi()) {
                    wifi_backoff_ms = 15000;
                    // First connect: kick the clock so MQTT TLS validity checks
                    // pass (cert dates need a sane RTC). Subsequent re-syncs
                    // are handled by g_timeManager->update() below.
                    if (!ntp_synced_once && g_timeManager) {
                        if (g_timeManager->syncNow()) {
                            sendStatus(NetworkStatus::Event::NTP_SYNCED, 0, "ntp ok");
                        }
                        ntp_synced_once = true;
                    }
                } else {
                    wifi_backoff_ms = wifi_backoff_ms < 30000 ? wifi_backoff_ms * 2 : 60000;
                    next_wifi_retry = now + wifi_backoff_ms;
                }
            }
        }

        // Periodic re-sync (TimeManager guards the 6h interval internally).
        if (g_timeManager && WiFi.status() == WL_CONNECTED) {
            g_timeManager->update();
        }

        // MQTT maintenance (only attempt with WiFi up). 5s retry on failure.
        bool wifi_up = (WiFi.status() == WL_CONNECTED);
        if (wifi_up && !mqtt.isConnected()) {
            if (now >= next_mqtt_retry) {
                sendStatus(NetworkStatus::Event::MQTT_CONNECTING, 0,
                           g_networkConfig->getMQTT().broker);
                if (mqtt.connect()) {
                    sendStatus(NetworkStatus::Event::MQTT_CONNECTED, 0,
                               g_networkConfig->getMQTT().client_id);
                    shadow.subscribe();
                    shadow.publishStateSnapshot();
                    mqtt_was_connected = true;
                } else {
                    sendStatus(NetworkStatus::Event::MQTT_DISCONNECTED, 0, "connect failed");
                    next_mqtt_retry = now + 5000;
                }
            }
        }
        if (mqtt_was_connected && !mqtt.isConnected()) {
            // Just dropped — notify UI; reconnect handled above on next tick.
            sendStatus(NetworkStatus::Event::MQTT_DISCONNECTED, 0, "lost");
            mqtt_was_connected = false;
        }

        // Drain the event queue. Non-blocking; we always come back through the
        // outer tick within ~50ms so latency for an event in steady state is
        // bounded by that.
        SessionEventMessage ev;
        while (xQueueReceive(g_sessionEventQueue, &ev, 0) == pdTRUE) {
            if (webhooks_present && wifi_up) {
                dispatcher.dispatch(g_networkConfig->getWebhooks(), ev);
            }
            if (mqtt.isConnected()) {
                shadow.publishOnEvent(ev);
            }
        }

        // Keepalive + incoming delta callbacks.
        if (mqtt.isConnected()) {
            mqtt.loop();
        }

        // 50ms tick — keeps MQTT responsive without burning CPU.
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

}  // namespace

void networkTask(void* parameter) {
    Serial.println("[NetworkTask] Starting on Core 1");
    Serial.printf("[NetworkTask] Stack high-water at entry: %d bytes\n",
                  uxTaskGetStackHighWaterMark(NULL) * 4);
    g_networkTaskHandle = xTaskGetCurrentTaskHandle();

    if (!g_networkConfig || !g_networkConfig->isValid()) {
        Serial.println("[NetworkTask] No network config; idle loop");
        while (true) vTaskDelay(pdMS_TO_TICKS(60000));
    }

    const char* device_id = g_networkConfig->getMQTT().client_id[0]
                          ? g_networkConfig->getMQTT().client_id
                          : "m5-pomodoro";
    WebhookDispatcher dispatcher(device_id);

    bool mqtt_enabled = g_networkConfig->getCloudSync().enabled &&
                        g_networkConfig->getMQTT().broker[0] != '\0' &&
                        g_networkConfig->getRootCA() != nullptr &&
                        g_networkConfig->getDeviceCert() != nullptr &&
                        g_networkConfig->getPrivateKey() != nullptr &&
                        g_stateMachine != nullptr;
    bool has_webhooks = g_networkConfig->getWebhooks().count > 0;

    if (!mqtt_enabled && !has_webhooks) {
        Serial.println("[NetworkTask] Neither MQTT nor webhooks configured; idle loop");
        while (true) vTaskDelay(pdMS_TO_TICKS(60000));
    }

    if (mqtt_enabled) {
        runPersistentLoop(dispatcher);
    } else {
        runOnDemandLoop(dispatcher);
    }
}
