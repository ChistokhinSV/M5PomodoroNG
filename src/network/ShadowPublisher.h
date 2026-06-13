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
    // Fires when the shadow's desired.task_name field changes. Implementation
    // (typically wired in NetworkTask) routes this into MainScreen so the
    // device LCD shows the project name driven by the server-side
    // task-context consumer.
    using TaskNameCallback = std::function<void(const char* name)>;

    ShadowPublisher(MQTTClient& mqtt,
                    const char* thing_name,
                    TimerStateMachine& sm,
                    PomodoroSequence& seq,
                    Statistics* stats);

    void setTaskNameCallback(TaskNameCallback cb) { task_name_cb_ = std::move(cb); }

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

    // Random id stamped into every reported snapshot. Server-side shadow-relay
    // emits a device.wake event when this id changes, which is the trigger
    // for consumer-wake-resync to look up any running Toggl entry and feed
    // the device a start command with the right remaining-seconds offset.
    uint32_t wake_id_;

    // Last delta command id we acted on; echoed in the next reported state so
    // the AWS shadow service stops re-delivering the delta. Empty when none.
    // 64 bytes is enough for the synthesized "<thing>:<epoch>:<detail-type>"
    // ids the server emits today (max ~55 chars). At 40 bytes it truncated
    // and the desired/reported diff never cleared.
    char last_command_id_[64] = "";
    char last_command_[16] = "";

    // Last task_name seen from desired-state delta. Echoed in reported so the
    // AWS shadow stops re-delivering and so the server-side can confirm the
    // device picked up the new name.
    char last_task_name_[64] = "";
    TaskNameCallback task_name_cb_;

    // Last seen remaining_sec_override from a desired delta. Echoed in
    // reported so the shadow's desired/reported diff clears once we've
    // received it — without this, every wake-resync push would leave a
    // perpetual delta of just {remaining_sec_override}, which carries no
    // command verb and ends up logged as "Delta: nothing actionable".
    uint32_t last_remaining_sec_override_ = 0;

    // project_color is written by consumer-toggl-api alongside task_name
    // (so a future GCal renderer can tint events). The firmware doesn't
    // render it today, but echoing it back is what clears the AWS
    // desired/reported diff — without this the device sees a perpetual
    // {project_color: "..."} delta logged as "nothing actionable" after
    // every legitimate task_name update. 8 bytes fits "#RRGGBB" + NUL.
    char last_project_color_[8] = "";

    // Last state-change source we relayed to the cloud. Always echoed back
    // in reported so the cloud-side shadow_parser can attribute device
    // transitions to button presses vs shadow-driven commands (the latter
    // must NOT cause toggl-api to auto-restart a Toggl entry on resume).
    // Values: "device" / "shadow_command".
    char last_state_change_source_[16] = "device";

    // Helpers for building / posting the JSON payload.
    void renderReported(char* out, size_t out_size, const SessionEventMessage* ev);
    void handleShadowDelta(const char* json, size_t len);

    static const char* stateName(TimerStateMachine::State s);
    static const char* sessionTypeName(uint8_t t);
};

#endif // SHADOW_PUBLISHER_H
