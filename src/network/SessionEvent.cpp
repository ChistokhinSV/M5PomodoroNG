#include "SessionEvent.h"
#include <Arduino.h>
#include <string.h>
#include <ctype.h>

const char* sessionEventName(SessionEvent e) {
    switch (e) {
        case SessionEvent::WORK_COMPLETE:  return "work_complete";
        case SessionEvent::BREAK_COMPLETE: return "break_complete";
        case SessionEvent::CYCLE_COMPLETE: return "cycle_complete";
        case SessionEvent::NONE:           return "none";
    }
    return "unknown";
}

namespace {
struct NameMask { const char* name; uint8_t mask; };
constexpr NameMask EVENT_TABLE[] = {
    {"work_complete",  static_cast<uint8_t>(SessionEvent::WORK_COMPLETE)},
    {"break_complete", static_cast<uint8_t>(SessionEvent::BREAK_COMPLETE)},
    {"cycle_complete", static_cast<uint8_t>(SessionEvent::CYCLE_COMPLETE)},
};

// Lowercase, trim leading/trailing whitespace in place. Returns the new length.
size_t trimLower(char* s) {
    if (!s) return 0;
    // Trim leading
    char* start = s;
    while (*start && isspace(static_cast<unsigned char>(*start))) ++start;
    // Trim trailing
    size_t len = strlen(start);
    while (len > 0 && isspace(static_cast<unsigned char>(start[len - 1]))) --len;
    start[len] = '\0';
    // Shift if needed
    if (start != s) memmove(s, start, len + 1);
    // Lowercase
    for (size_t i = 0; i < len; ++i) s[i] = static_cast<char>(tolower(static_cast<unsigned char>(s[i])));
    return len;
}
}  // namespace

uint8_t parseSessionEventMask(const char* csv) {
    if (!csv || !*csv) return SESSION_EVENT_ALL;

    // Wildcard short-circuit
    {
        // Skip leading whitespace
        const char* p = csv;
        while (*p && isspace(static_cast<unsigned char>(*p))) ++p;
        if (*p == '*') return SESSION_EVENT_ALL;
    }

    uint8_t mask = 0;
    // Mutable copy for tokenizing; cap at a sensible size.
    char buf[128];
    strncpy(buf, csv, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* save = nullptr;
    for (char* tok = strtok_r(buf, ",", &save); tok; tok = strtok_r(nullptr, ",", &save)) {
        if (trimLower(tok) == 0) continue;
        bool matched = false;
        for (const auto& row : EVENT_TABLE) {
            if (strcmp(row.name, tok) == 0) {
                mask |= row.mask;
                matched = true;
                break;
            }
        }
        if (!matched) {
            Serial.printf("[SessionEvent] WARN: unknown event \"%s\" ignored\n", tok);
        }
    }

    if (mask == 0) {
        // Nothing valid was given — treat as "match nothing" so the user notices.
        Serial.println("[SessionEvent] WARN: empty event mask, webhook will not fire");
    }
    return mask;
}
