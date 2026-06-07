#include "ShadowPublisher.h"
#include "../core/TimeManager.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_random.h>
#include <string.h>
#include <time.h>

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "0.0.0"
#endif

// Shadow timestamps must be true UTC epoch. M5Unified copies the BM8563 RTC
// into the ESP32 system clock at boot, but the BM8563 stores local wall-clock
// time, so raw time(nullptr) returns local-time-as-UTC. TimeManager reads
// BM8563 via mktime with TZ applied, producing real UTC.
extern TimeManager* g_timeManager;

static uint32_t nowEpochUtc() {
    if (g_timeManager) return g_timeManager->getEpoch();
    return static_cast<uint32_t>(time(nullptr));
}

ShadowPublisher::ShadowPublisher(MQTTClient& mqtt,
                                 const char* thing_name,
                                 TimerStateMachine& sm,
                                 PomodoroSequence& seq,
                                 Statistics* stats)
    : mqtt_(mqtt), sm_(sm), seq_(seq), stats_(stats) {
    // Defensive: thing_name may be null or empty. Shadow topics still resolve
    // (AWS will reject auth) but at least we don't crash.
    const char* tn = (thing_name && thing_name[0]) ? thing_name : "unknown";
    strncpy(thing_name_, tn, sizeof(thing_name_) - 1);
    thing_name_[sizeof(thing_name_) - 1] = '\0';

    snprintf(topic_update_, sizeof(topic_update_),
             "$aws/things/%s/shadow/update", thing_name_);
    snprintf(topic_delta_, sizeof(topic_delta_),
             "$aws/things/%s/shadow/update/delta", thing_name_);

    // Per-boot id. esp_random returns a hardware-RNG uint32; we keep only the
    // bottom 24 bits so the value renders compactly in shadow JSON and logs.
    wake_id_ = esp_random() & 0xFFFFFFu;
}

bool ShadowPublisher::subscribe() {
    bool ok = mqtt_.subscribe(topic_delta_);
    Serial.printf("[Shadow] Subscribe %s: %s\n", topic_delta_, ok ? "OK" : "FAIL");
    return ok;
}

bool ShadowPublisher::publishStateSnapshot() {
    char body[640];
    renderReported(body, sizeof(body), nullptr);
    bool ok = mqtt_.publish(topic_update_, body);
    Serial.printf("[Shadow] Snapshot publish: %s (%u bytes)\n",
                  ok ? "OK" : "FAIL", static_cast<unsigned>(strlen(body)));
    return ok;
}

bool ShadowPublisher::publishOnEvent(const SessionEventMessage& ev) {
    char body[640];
    renderReported(body, sizeof(body), &ev);
    bool ok = mqtt_.publish(topic_update_, body);
    Serial.printf("[Shadow] Event %s -> shadow: %s (%u bytes)\n",
                  sessionEventName(ev.type), ok ? "OK" : "FAIL",
                  static_cast<unsigned>(strlen(body)));
    return ok;
}

void ShadowPublisher::handleMqttMessage(const char* topic, const char* payload, size_t len) {
    if (!topic) return;
    if (strcmp(topic, topic_delta_) == 0) {
        handleShadowDelta(payload, len);
        return;
    }
    Serial.printf("[Shadow] Ignored message on %s (%u bytes)\n", topic, static_cast<unsigned>(len));
}

void ShadowPublisher::renderReported(char* out, size_t out_size, const SessionEventMessage* ev) {
    // Build {"state":{"reported":{...}}}. Prefer fields from the event when
    // present (richer than current state for completion events); fall back to
    // a fresh query of the state machine + sequence + statistics.
    JsonDocument doc;
    auto reported = doc["state"]["reported"].to<JsonObject>();

    reported["fw_version"] = FIRMWARE_VERSION;
    // wake_id is constant for the lifetime of this firmware boot. A change
    // here is the server's signal that the device just came online; the
    // wake-resync consumer reads it to decide whether to feed an offset.
    reported["wake_id"] = wake_id_;

    TimerStateMachine::State cur_state = sm_.getState();
    reported["device_state"] = stateName(cur_state);

    auto cur_session = seq_.getCurrentSession();
    reported["session_type"]   = sessionTypeName(static_cast<uint8_t>(cur_session.type));
    reported["session_number"] = seq_.getCurrentWorkSession();
    reported["total_sessions"] = seq_.getTotalWorkSessions();
    reported["duration_min"]   = cur_session.duration_min;

    // remaining_sec only meaningful during ACTIVE / PAUSED
    if (cur_state != TimerStateMachine::State::IDLE) {
        reported["remaining_sec"] = sm_.getRemainingMs() / 1000U;
    } else {
        reported["remaining_sec"] = 0;
    }

    if (stats_) {
        auto today = stats_->getToday();
        reported["today"]    = today.completed_sessions;
        reported["week"]     = stats_->getLast7DaysTotal();
        reported["lifetime"] = stats_->getTotalCompleted();
    }

    reported["timestamp"] = nowEpochUtc();

    if (ev) {
        reported["last_event"]    = sessionEventName(ev->type);
        reported["last_event_at"] = ev->timestamp;
        // Override "current" fields with the event's snapshot where it carried
        // them — the state machine has already moved on by the time the
        // network task drains the queue.
        if (ev->today_count > 0) reported["today"] = ev->today_count;
        if (ev->week_count  > 0) reported["week"]  = ev->week_count;
    }

    // Echo the last accepted delta command so AWS shadow clears the diff
    // between desired and reported.
    if (last_command_[0]) {
        reported["command"] = last_command_;
    }
    if (last_command_id_[0]) {
        reported["command_id"] = last_command_id_;
    }
    if (last_task_name_[0]) {
        // Echo task_name so a server-side change to desired clears once we
        // applied it locally. Server's task-context consumer can also
        // verify the device actually picked it up.
        reported["task_name"] = last_task_name_;
    }
    if (last_remaining_sec_override_ > 0) {
        // Echo so AWS clears the desired/reported diff. Without this the
        // shadow keeps re-publishing a {remaining_sec_override} delta with
        // no command — device logs it as "Delta: nothing actionable".
        reported["remaining_sec_override"] = last_remaining_sec_override_;
    }

    size_t n = serializeJson(doc, out, out_size);
    if (n == 0 || n >= out_size) {
        // Truncated — return a minimal payload rather than malformed JSON.
        snprintf(out, out_size, "{\"state\":{\"reported\":{\"error\":\"truncated\"}}}");
    }
}

void ShadowPublisher::handleShadowDelta(const char* json, size_t len) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json, len);
    if (err) {
        Serial.printf("[Shadow] Delta parse error: %s\n", err.c_str());
        return;
    }
    // AWS sends {"state":{ <only-fields-that-differ-from-reported> }}. Any
    // combination of task_name, command_id and command may be present. Treat
    // each field independently: echoing it back in reported is what clears
    // the AWS shadow diff, even if no `command` verb was sent (e.g. a
    // server-side task-context update that only touches task_name).
    bool any_change = false;

    // --- task_name -------------------------------------------------------
    const char* task_name = doc["state"]["task_name"] | (const char*)nullptr;
    if (task_name && *task_name &&
        strncmp(task_name, last_task_name_, sizeof(last_task_name_)) != 0) {
        strncpy(last_task_name_, task_name, sizeof(last_task_name_) - 1);
        last_task_name_[sizeof(last_task_name_) - 1] = '\0';
        Serial.printf("[Shadow] Delta task_name=\"%s\"\n", last_task_name_);
        if (task_name_cb_) task_name_cb_(last_task_name_);
        any_change = true;
    }

    // --- command_id ------------------------------------------------------
    // Updated even when no command verb arrived (server may have re-sent the
    // same verb with a new id). Echoing alone clears AWS's delta on the id.
    const char* cid = doc["state"]["command_id"] | (const char*)nullptr;
    if (cid && *cid &&
        strncmp(cid, last_command_id_, sizeof(last_command_id_)) != 0) {
        strncpy(last_command_id_, cid, sizeof(last_command_id_) - 1);
        last_command_id_[sizeof(last_command_id_) - 1] = '\0';
        any_change = true;
    }

    // --- remaining_sec_override ----------------------------------------
    // Always parsed, even when no command came with it — if AWS sends a
    // delta of just {remaining_sec_override} (e.g. command_id and command
    // already matched reported), we still need to echo the value back so
    // the desired/reported diff converges. The actual *apply* only happens
    // alongside a command=start below.
    uint32_t remaining_override_sec = doc["state"]["remaining_sec_override"] | 0U;
    if (remaining_override_sec != last_remaining_sec_override_) {
        last_remaining_sec_override_ = remaining_override_sec;
        any_change = true;
        Serial.printf("[Shadow] remaining_sec_override = %u\n",
                      remaining_override_sec);
    }

    // --- command ---------------------------------------------------------
    const char* cmd = doc["state"]["command"] | (const char*)nullptr;
    if (cmd && *cmd) {

        Serial.printf("[Shadow] Delta command=\"%s\" id=\"%s\"%s\n",
                      cmd, cid ? cid : "",
                      remaining_override_sec
                          ? (" override=" + String(remaining_override_sec) + "s").c_str()
                          : "");

        using Event = TimerStateMachine::Event;
        bool handled = true;
        if (strcmp(cmd, "start") == 0) {
            sm_.handleEvent(Event::START);
            // After START, total_ms is set to the configured duration and
            // remaining_ms equals it. If the server gave us an offset, snap
            // remaining_ms to it. restoreState() preserves state=ACTIVE and
            // doesn't re-fire start-time hooks (audio/LED) which already
            // ran via handleEvent above — exactly what we want.
            if (remaining_override_sec > 0) {
                uint32_t total_ms = sm_.getTotalMs();
                uint32_t remaining_ms = remaining_override_sec * 1000U;
                if (remaining_ms > total_ms) remaining_ms = total_ms;
                sm_.restoreState(TimerStateMachine::State::ACTIVE,
                                 remaining_ms, total_ms);
                Serial.printf("[Shadow] Applied remaining_sec_override: "
                              "remaining_ms=%u total_ms=%u\n",
                              remaining_ms, total_ms);
            }
        }
        else if (strcmp(cmd, "pause")  == 0) sm_.handleEvent(Event::PAUSE);
        else if (strcmp(cmd, "resume") == 0) sm_.handleEvent(Event::RESUME);
        else if (strcmp(cmd, "skip")   == 0) sm_.handleEvent(Event::SKIP);
        else if (strcmp(cmd, "stop")   == 0) sm_.handleEvent(Event::STOP);
        else {
            Serial.printf("[Shadow] Unknown delta command: %s\n", cmd);
            handled = false;
        }
        if (handled &&
            strncmp(cmd, last_command_, sizeof(last_command_)) != 0) {
            strncpy(last_command_, cmd, sizeof(last_command_) - 1);
            last_command_[sizeof(last_command_) - 1] = '\0';
            any_change = true;
        }
    }

    if (any_change) {
        publishStateSnapshot();
    } else {
        Serial.println("[Shadow] Delta: nothing actionable, ignoring");
    }
}

const char* ShadowPublisher::stateName(TimerStateMachine::State s) {
    switch (s) {
        case TimerStateMachine::State::IDLE:   return "idle";
        case TimerStateMachine::State::ACTIVE: return "active";
        case TimerStateMachine::State::PAUSED: return "paused";
    }
    return "unknown";
}

const char* ShadowPublisher::sessionTypeName(uint8_t t) {
    switch (static_cast<PomodoroSequence::SessionType>(t)) {
        case PomodoroSequence::SessionType::WORK:        return "work";
        case PomodoroSequence::SessionType::SHORT_BREAK: return "short_break";
        case PomodoroSequence::SessionType::LONG_BREAK:  return "long_break";
    }
    return "unknown";
}
