#ifndef WEBHOOK_DISPATCHER_H
#define WEBHOOK_DISPATCHER_H

#include "../core/NetworkConfig.h"
#include "SessionEvent.h"

/**
 * Fires SessionEventMessage payloads at user-configured webhook URLs.
 *
 * Architecture (current, on-demand WiFi):
 *   NetworkTask receives SessionEventMessage from g_sessionEventQueue,
 *   ensures WiFi is up, then calls dispatch(event). dispatch() iterates the
 *   configured WebhookEndpoints, applies the per-endpoint event-mask filter,
 *   builds a small JSON body via ArduinoJson, and POSTs sequentially on a
 *   single WiFi session.
 *
 * Transport:
 *   - http:// and https:// both supported via HTTPClient + WiFiClient(Secure).
 *   - HTTPS uses setInsecure(): user webhook URLs aren't pinned to specific
 *     CAs (this isn't the AWS IoT shadow path). Acceptable trade-off for
 *     user-controlled endpoints; revisit if pinning is needed later.
 *
 * Persistence-readiness:
 *   This class is stateless — no sockets held between calls. When the
 *   network mode flips to persistent (for MQTT shadow), dispatch() still
 *   works the same; the caller just doesn't tear WiFi down between events.
 */
class WebhookDispatcher {
public:
    // device_id is embedded in the JSON payload's "device" field. Pointer is
    // stored by reference — keep the underlying buffer alive for the
    // dispatcher's lifetime (e.g. NetworkConfig::MQTTSettings::client_id).
    explicit WebhookDispatcher(const char* device_id);

    // Fire one event at every enabled endpoint whose event_mask matches.
    // Returns the number of webhooks that received a 2xx response.
    uint8_t dispatch(const NetworkConfig::WebhookSettings& cfg,
                     const SessionEventMessage& event);

    // Copy `url` into `out`, replacing any "/bot<TOKEN>/" segment (Telegram
    // bot URLs) with "/bot***/". Logs that echo URLs should pass through
    // here so a leaked log doesn't leak the bot token.
    static void maskUrl(const char* url, char* out, size_t out_size);

private:
    const char* device_id_;

    // Returns HTTP status code, or negative on transport error. Body is the
    // marshaled JSON payload (not retained between calls).
    int postOne(const NetworkConfig::WebhookEndpoint& endpoint, const char* body);

    // Substitute {event}, {duration_min}, {session_number}, {total_sessions},
    // {today}, {week}, {device} into the template into `out`. Unknown {names}
    // are passed through verbatim. Caller owns `out`. Returns chars written.
    size_t renderTemplate(const char* tmpl,
                          const SessionEventMessage& event,
                          char* out, size_t out_size) const;

    // Pick a reasonable default text when the user didn't supply Text=. Used
    // for Format=telegram so a misconfigured webhook still emits something.
    const char* defaultTemplate(SessionEvent type) const;
};

#endif // WEBHOOK_DISPATCHER_H
