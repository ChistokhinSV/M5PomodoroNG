#include "WebhookDispatcher.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <string.h>
#include <stdlib.h>

WebhookDispatcher::WebhookDispatcher(const char* device_id)
    : device_id_(device_id ? device_id : "") {}

void WebhookDispatcher::maskUrl(const char* url, char* out, size_t out_size) {
    if (!out || out_size == 0) return;
    if (!url) { out[0] = '\0'; return; }

    // Find "/bot" then the next "/" after it. Everything between gets masked.
    const char* bot = strstr(url, "/bot");
    if (!bot) {
        strncpy(out, url, out_size - 1);
        out[out_size - 1] = '\0';
        return;
    }
    const char* slash = strchr(bot + 4, '/');
    if (!slash) {
        strncpy(out, url, out_size - 1);
        out[out_size - 1] = '\0';
        return;
    }

    size_t prefix_len = static_cast<size_t>(bot - url) + 4;  // include "/bot"
    if (prefix_len >= out_size) prefix_len = out_size - 1;
    memcpy(out, url, prefix_len);
    size_t written = prefix_len;

    const char mask[] = "***";
    for (size_t i = 0; i < sizeof(mask) - 1 && written + 1 < out_size; ++i) {
        out[written++] = mask[i];
    }
    while (*slash && written + 1 < out_size) out[written++] = *slash++;
    out[written] = '\0';
}

const char* WebhookDispatcher::defaultTemplate(SessionEvent type) const {
    // ASCII-only defaults so we don't surprise users on non-UTF8 terminals.
    // Users wanting emoji can supply Text= explicitly in network.ini.
    switch (type) {
        case SessionEvent::WORK_COMPLETE:
            return "Pomodoro work {session_number}/{total_sessions} done, today {today}";
        case SessionEvent::BREAK_COMPLETE:
            return "Break done";
        case SessionEvent::CYCLE_COMPLETE:
            return "Pomodoro cycle complete, today {today}";
        case SessionEvent::NONE:
        default:
            return "Pomodoro event";
    }
}

size_t WebhookDispatcher::renderTemplate(const char* tmpl,
                                         const SessionEventMessage& event,
                                         char* out, size_t out_size) const {
    if (!tmpl || !out || out_size == 0) return 0;

    size_t written = 0;
    auto append_bytes = [&](const char* s, size_t n) {
        if (!s) return;
        while (n-- && written + 1 < out_size) out[written++] = *s++;
    };
    auto append_str  = [&](const char* s) { if (s) append_bytes(s, strlen(s)); };
    auto append_uint = [&](unsigned v) {
        char buf[12];
        int n = snprintf(buf, sizeof(buf), "%u", v);
        if (n > 0) append_bytes(buf, static_cast<size_t>(n));
    };

    const char* p = tmpl;
    while (*p) {
        if (*p == '{') {
            const char* end = strchr(p + 1, '}');
            size_t name_len = end ? static_cast<size_t>(end - (p + 1)) : 0;
            if (end && name_len > 0 && name_len < 32) {
                char name[32];
                memcpy(name, p + 1, name_len);
                name[name_len] = '\0';
                bool handled = true;
                if      (strcmp(name, "event")          == 0) append_str(sessionEventName(event.type));
                else if (strcmp(name, "duration_min")   == 0) append_uint(event.duration_min);
                else if (strcmp(name, "session_number") == 0) append_uint(event.session_number);
                else if (strcmp(name, "total_sessions") == 0) append_uint(event.total_sessions);
                else if (strcmp(name, "today")          == 0) append_uint(event.today_count);
                else if (strcmp(name, "week")           == 0) append_uint(event.week_count);
                else if (strcmp(name, "device")         == 0) append_str(device_id_ && device_id_[0] ? device_id_ : "m5-pomodoro");
                else handled = false;
                if (handled) { p = end + 1; continue; }
                // Unknown placeholder name -> fall through, emit literal {name}
            }
            // Not a closed/known placeholder: emit literal '{' and continue.
        }
        if (written + 1 < out_size) out[written++] = *p;
        ++p;
    }
    out[written] = '\0';
    return written;
}

uint8_t WebhookDispatcher::dispatch(const NetworkConfig::WebhookSettings& cfg,
                                    const SessionEventMessage& event) {
    if (cfg.count == 0) return 0;
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[Webhook] WiFi not connected; skipping dispatch");
        return 0;
    }

    uint8_t event_bit = static_cast<uint8_t>(event.type);
    uint8_t ok = 0;
    char body[384];  // Big enough for Telegram payload with 256-char Text.

    for (uint8_t i = 0; i < cfg.count; ++i) {
        const auto& ep = cfg.endpoints[i];
        if (!ep.enabled) continue;
        // event_mask is a bitmask of SessionEvent values. SESSION_EVENT_ALL
        // (0xFF) matches everything, including future event types.
        if ((ep.event_mask & event_bit) == 0) continue;

        size_t body_len = 0;
        if (ep.format == NetworkConfig::WebhookFormat::TELEGRAM) {
            if (ep.chat_id[0] == '\0') {
                Serial.printf("[Webhook] Skip %s -> %s: Format=telegram but no ChatID\n",
                              sessionEventName(event.type), ep.url);
                continue;
            }
            // Render text. Empty Text= falls back to a sensible per-event default.
            const char* tmpl = ep.text_template[0] ? ep.text_template
                                                   : defaultTemplate(event.type);
            char text_buf[256];
            renderTemplate(tmpl, event, text_buf, sizeof(text_buf));

            // Telegram expects {"chat_id": <int|string>, "text": "<utf8>"}.
            // Try parsing chat_id as int64 (personal: positive, groups: negative);
            // fall back to string for "@channel"-style identifiers.
            JsonDocument doc;
            char* end = nullptr;
            long long chat = strtoll(ep.chat_id, &end, 10);
            if (end != ep.chat_id && *end == '\0') {
                doc["chat_id"] = chat;
            } else {
                doc["chat_id"] = ep.chat_id;
            }
            doc["text"] = text_buf;
            body_len = serializeJson(doc, body, sizeof(body));
        } else {
            // Generic JSON shape — same payload that ships to non-Telegram URLs.
            JsonDocument doc;
            doc["event"]          = sessionEventName(event.type);
            doc["device"]         = device_id_ && device_id_[0] ? device_id_ : "m5-pomodoro";
            doc["timestamp"]      = event.timestamp;
            doc["duration_min"]   = event.duration_min;
            doc["session_number"] = event.session_number;
            doc["total_sessions"] = event.total_sessions;
            doc["today"]          = event.today_count;
            doc["week"]           = event.week_count;
            body_len = serializeJson(doc, body, sizeof(body));
        }

        char masked[208];
        maskUrl(ep.url, masked, sizeof(masked));

        if (body_len == 0 || body_len >= sizeof(body)) {
            Serial.printf("[Webhook] ERROR: payload serialization failed for %s\n", masked);
            continue;
        }

        int status = postOne(ep, body);
        if (status >= 200 && status < 300) {
            ++ok;
            Serial.printf("[Webhook] %s -> %s : HTTP %d\n",
                          sessionEventName(event.type), masked, status);
        } else {
            Serial.printf("[Webhook] %s -> %s : FAIL (%d)\n",
                          sessionEventName(event.type), masked, status);
        }
    }
    return ok;
}

int WebhookDispatcher::postOne(const NetworkConfig::WebhookEndpoint& endpoint,
                               const char* body) {
    // Pick the right transport. HTTPClient owns the client by reference, so
    // both must outlive the request — keep them on the stack here.
    HTTPClient http;
    WiFiClient plain;
    WiFiClientSecure tls;
    bool is_https = (strncmp(endpoint.url, "https://", 8) == 0);

    bool started = false;
    if (is_https) {
        // No cert pinning for user-provided URLs in this slice — server-side
        // app + MQTT shadow path will get proper certs in the next slice.
        tls.setInsecure();
        started = http.begin(tls, endpoint.url);
    } else {
        started = http.begin(plain, endpoint.url);
    }
    if (!started) {
        char masked[208];
        maskUrl(endpoint.url, masked, sizeof(masked));
        Serial.printf("[Webhook] http.begin failed for %s\n", masked);
        return -1;
    }

    http.setTimeout(8000);  // 8s — webhook receivers should respond fast
    http.addHeader("Content-Type", "application/json");
    http.addHeader("User-Agent", "M5Pomodoro/2.0");
    if (endpoint.auth_header[0]) {
        http.addHeader("Authorization", endpoint.auth_header);
    }

    // HTTPClient::POST takes a non-const uint8_t*; cast away const since the
    // call does not actually mutate the buffer.
    uint8_t* buf = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(body));
    int status = http.POST(buf, strlen(body));

    // On non-2xx, surface the response body so Telegram's error JSON
    // ("Bad Request: chat not found", "Unauthorized", etc.) is visible.
    // Skip on success to keep the log quiet during normal operation.
    if (!(status >= 200 && status < 300)) {
        String resp = http.getString();
        if (resp.length() > 0) {
            if (resp.length() > 200) {
                resp = resp.substring(0, 200) + "...";
            }
            Serial.printf("[Webhook] response body: %s\n", resp.c_str());
        }
    }

    http.end();
    return status;
}
