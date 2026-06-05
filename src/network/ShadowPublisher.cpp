#include "ShadowPublisher.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <string.h>
#include <time.h>

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "0.0.0"
#endif

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

    reported["timestamp"] = static_cast<uint32_t>(time(nullptr));

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
    // AWS sends {"version":N,"timestamp":...,"state":{"command":"..."}}
    const char* cmd = doc["state"]["command"] | (const char*)nullptr;
    const char* cid = doc["state"]["command_id"] | (const char*)nullptr;
    if (!cmd || !*cmd) {
        Serial.println("[Shadow] Delta: no command field, ignoring");
        return;
    }
    Serial.printf("[Shadow] Delta command=\"%s\" id=\"%s\"\n", cmd, cid ? cid : "");

    using Event = TimerStateMachine::Event;
    bool handled = true;
    if      (strcmp(cmd, "start")  == 0) sm_.handleEvent(Event::START);
    else if (strcmp(cmd, "pause")  == 0) sm_.handleEvent(Event::PAUSE);
    else if (strcmp(cmd, "resume") == 0) sm_.handleEvent(Event::RESUME);
    else if (strcmp(cmd, "skip")   == 0) sm_.handleEvent(Event::SKIP);
    else if (strcmp(cmd, "stop")   == 0) sm_.handleEvent(Event::STOP);
    else {
        Serial.printf("[Shadow] Unknown delta command: %s\n", cmd);
        handled = false;
    }

    if (handled) {
        // Remember which command/id we accepted so the next reported payload
        // echoes it back and AWS clears the desired/reported diff.
        strncpy(last_command_, cmd, sizeof(last_command_) - 1);
        last_command_[sizeof(last_command_) - 1] = '\0';
        if (cid) {
            strncpy(last_command_id_, cid, sizeof(last_command_id_) - 1);
            last_command_id_[sizeof(last_command_id_) - 1] = '\0';
        } else {
            last_command_id_[0] = '\0';
        }
        publishStateSnapshot();
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
