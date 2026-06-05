#ifndef SHADOW_PUBLISHER_H
#define SHADOW_PUBLISHER_H

#include "MQTTClient.h"
#include "SessionEvent.h"
#include "../core/TimerStateMachine.h"
#include "../core/PomodoroSequence.h"
#include "../core/Statistics.h"

/**
 * Translates SessionEventMessage and direct state queries into AWS IoT Device
 * Shadow updates ("reported" state) and routes incoming /delta messages back
 * into TimerStateMachine commands.
 *
 * Topics (lazily formatted from thing_name):
 *   publish   $aws/things/<thing>/shadow/update
 *   subscribe $aws/things/<thing>/shadow/update/delta
 *
 * Reported state schema:
 *   { "state": { "reported": {
 *       "fw_version": "...", "device_state": "active|paused|idle",
 *       "session_type": "work|short_break|long_break",
 *       "session_number": N, "total_sessions": N, "duration_min": N,
 *       "remaining_sec": N, "today": N, "week": N, "lifetime": N,
 *       "last_event": "...", "last_event_at": <epoch>,
 *       "command": "..." (echo after acting on a delta)
 *   } } }
 *
 * Delta convention (server -> device):
 *   { "state": { "command": "start|pause|resume|skip|stop",
 *                "command_id": "<optional, echoed in next reported>" } }
 */
class ShadowPublisher {
public:
    ShadowPublisher(MQTTClient& mqtt,
                    const char* thing_name,
                    TimerStateMachine& sm,
                    PomodoroSequence& seq,
                    Statistics* stats);

    // Subscribe to /shadow/update/delta. Call once after MQTT connect succeeds.
    bool subscribe();

    // Publish a full reported state. Use at boot (after connect) and after a
    // delta-driven action so the desired/reported diff clears.
    bool publishStateSnapshot();

    // Publish a reported state derived from the just-finished event. Called
    // from NetworkTask for every SessionEventMessage drained off the queue.
    bool publishOnEvent(const SessionEventMessage& ev);

    // MQTT callback hook — install via mqtt_.onMessage([&](...){
    // shadow_.handleMqttMessage(...); }).
    void handleMqttMessage(const char* topic, const char* payload, size_t len);

private:
    MQTTClient& mqtt_;
    TimerStateMachine& sm_;
    PomodoroSequence& seq_;
    Statistics* stats_;

    char thing_name_[32];
    char topic_update_[80];   // $aws/things/<thing>/shadow/update
    char topic_delta_[80];    // $aws/things/<thing>/shadow/update/delta

    // Last delta command id we acted on; echoed in the next reported state so
    // the AWS shadow service stops re-delivering the delta. Empty when none.
    char last_command_id_[40] = "";
    char last_command_[16] = "";

    // Helpers for building / posting the JSON payload.
    void renderReported(char* out, size_t out_size, const SessionEventMessage* ev);
    void handleShadowDelta(const char* json, size_t len);

    static const char* stateName(TimerStateMachine::State s);
    static const char* sessionTypeName(uint8_t t);
};

#endif // SHADOW_PUBLISHER_H
