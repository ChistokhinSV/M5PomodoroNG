#ifndef SESSION_EVENT_H
#define SESSION_EVENT_H

#include <stdint.h>

// Canonical event stream emitted by TimerStateMachine when a session completes.
// Single queue with multiple subscribers in NetworkTask:
//   - WebhookDispatcher (this PR)            HTTPS POST per webhook URL
//   - ShadowSync (follow-up PR, future MQTT) AWS IoT shadow document update
//
// Bitmask values so webhooks can filter via Events= in network.ini.
enum class SessionEvent : uint8_t {
    NONE           = 0,
    WORK_COMPLETE  = 1 << 0,   // work session timed out
    BREAK_COMPLETE = 1 << 1,   // short or long break timed out
    CYCLE_COMPLETE = 1 << 2,   // last work of the cycle finished (also fires WORK_COMPLETE)
    STATE_CHANGED  = 1 << 3,   // TimerStateMachine entered a new state (IDLE/ACTIVE/PAUSED)
};

// Convenience: "subscribe to everything"
static constexpr uint8_t SESSION_EVENT_ALL = 0xFF;

// Producer payload (Core 0) → Consumer (Core 1). Timestamp is Unix epoch
// from M5.Rtc. device_state / session_type / remaining_sec are populated for
// STATE_CHANGED; completion events leave remaining_sec at 0.
//
// state_change_source: 0=device (button/gyro/timeout), 1=shadow_command
// (a delta verb drove the transition). The cloud-side shadow parser embeds
// this into the device.session.* event so toggl-api knows whether to
// auto-restart a Toggl entry on resume (yes for device, no for shadow).
struct SessionEventMessage {
    SessionEvent type;
    uint8_t  device_state;     // 0=IDLE, 1=ACTIVE, 2=PAUSED (TimerStateMachine::State)
    uint8_t  session_type;     // 0=WORK, 1=SHORT_BREAK, 2=LONG_BREAK (current session)
    uint8_t  session_number;   // 1-based work session inside the current cycle
    uint8_t  total_sessions;   // work sessions per cycle
    uint8_t  state_change_source;  // 0=device, 1=shadow_command (STATE_CHANGED only)
    uint16_t duration_min;     // duration of the relevant session
    uint16_t remaining_sec;    // for STATE_CHANGED + ACTIVE/PAUSED only
    uint16_t today_count;      // completed work sessions today (Statistics)
    uint16_t week_count;       // rolling 7-day total
    uint32_t timestamp;
};

// Human-readable name used for JSON payloads and log lines.
const char* sessionEventName(SessionEvent e);

// Parse a comma-separated event list ("work_complete,cycle_complete") into a
// bitmask. "*" or empty -> SESSION_EVENT_ALL. Unknown names are ignored with
// a warning to Serial.
uint8_t parseSessionEventMask(const char* csv);

#endif // SESSION_EVENT_H
