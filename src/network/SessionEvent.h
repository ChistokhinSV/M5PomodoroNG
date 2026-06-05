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
};

// Convenience: "subscribe to everything"
static constexpr uint8_t SESSION_EVENT_ALL = 0xFF;

// Producer payload (Core 0) → Consumer (Core 1). 16 bytes — fits comfortably
// in any FreeRTOS queue. Timestamp is Unix epoch from M5.Rtc.
struct SessionEventMessage {
    SessionEvent type;
    uint32_t timestamp;
    uint16_t duration_min;     // duration of the just-completed session
    uint8_t  session_number;   // 1-based within the current cycle (PomodoroSequence)
    uint8_t  total_sessions;   // sessions per cycle
    uint16_t today_count;      // completed work sessions today (Statistics)
    uint16_t week_count;       // rolling 7-day total
};

// Human-readable name used for JSON payloads and log lines.
const char* sessionEventName(SessionEvent e);

// Parse a comma-separated event list ("work_complete,cycle_complete") into a
// bitmask. "*" or empty -> SESSION_EVENT_ALL. Unknown names are ignored with
// a warning to Serial.
uint8_t parseSessionEventMask(const char* csv);

#endif // SESSION_EVENT_H
