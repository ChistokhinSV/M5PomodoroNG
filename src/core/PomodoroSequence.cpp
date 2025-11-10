#include "PomodoroSequence.h"
#include <Arduino.h>

PomodoroSequence::PomodoroSequence()
    : current_session(1),
      completed_today(0),
      sequence_start_epoch(0),
      custom_work_min(25),
      custom_short_break_min(5),
      custom_long_break_min(15),
      custom_sessions_before_long(4) {
}

void PomodoroSequence::setWorkDuration(uint16_t minutes) {
    custom_work_min = minutes;
}

void PomodoroSequence::setShortBreakDuration(uint16_t minutes) {
    custom_short_break_min = minutes;
}

void PomodoroSequence::setLongBreakDuration(uint16_t minutes) {
    custom_long_break_min = minutes;
}

void PomodoroSequence::setSessionsBeforeLong(uint8_t count) {
    custom_sessions_before_long = constrain(count, 2, 8);  // Reasonable limits
}

void PomodoroSequence::setNumCycles(uint8_t cycles) {
    custom_num_cycles = constrain(cycles, 1, 4);  // Reasonable limits (1-4 cycles)
}

void PomodoroSequence::start() {
    sequence_start_epoch = millis() / 1000;  // Approximate epoch (seconds since boot)
    current_session = 1;
}

void PomodoroSequence::reset() {
    current_session = 1;
    sequence_start_epoch = 0;
}

PomodoroSequence::Session PomodoroSequence::getCurrentSession() const {
    Session session;
    session.number = current_session;
    session.type = getSessionType(current_session);
    session.duration_min = getSessionDuration(session.type);
    return session;
}

PomodoroSequence::Session PomodoroSequence::getNextSession() const {
    uint8_t next_session = current_session + 1;
    uint8_t total = getTotalIntervals();

    // Wrap around after completing full cycle
    if (next_session > total) {
        next_session = 1;
    }

    Session session;
    session.number = next_session;
    session.type = getSessionType(next_session);
    session.duration_min = getSessionDuration(session.type);
    return session;
}

bool PomodoroSequence::advance() {
    uint8_t total = getTotalIntervals();
    current_session++;

    // Wrap around to 1 after completing full cycle
    bool cycle_completed = false;
    if (current_session > total) {
        current_session = 1;
        completed_today++;
        cycle_completed = true;
        Serial.println("[PomodoroSequence] Cycle completed! Starting new cycle");
    }

    return cycle_completed;
}

// MP-51: New methods to distinguish work sessions from intervals

uint8_t PomodoroSequence::getTotalWorkSessions() const {
    return custom_sessions_before_long * custom_num_cycles;
}

uint8_t PomodoroSequence::getCurrentWorkSession() const {
    // Count completed work sessions + 1 if currently in work session
    uint8_t completed_work = 0;

    // Count work sessions before current
    for (uint8_t i = 1; i < current_session; i++) {
        if (getSessionType(i) == SessionType::WORK) {
            completed_work++;
        }
    }

    // Add 1 if currently in work session
    if (isWorkSession()) {
        return completed_work + 1;
    }

    // If in break, return the last work session number
    return completed_work > 0 ? completed_work : 1;
}

uint8_t PomodoroSequence::getSessionsBeforeLong() const {
    return custom_sessions_before_long;
}

bool PomodoroSequence::isNextLongBreak() const {
    uint8_t next_session = current_session + 1;
    uint8_t total = getTotalIntervals();

    if (next_session > total) {
        next_session = 1;  // Wrap around
    }

    return getSessionType(next_session) == SessionType::LONG_BREAK;
}

uint8_t PomodoroSequence::getTotalIntervals() const {
    // Each cycle: sessions × 2 intervals (work + break)
    // Total: cycles × sessions × 2
    return custom_sessions_before_long * 2 * custom_num_cycles;
}

// Legacy method (for compatibility during refactor)
uint8_t PomodoroSequence::getTotalSessions() const {
    return getTotalIntervals();
}

bool PomodoroSequence::isWorkSession() const {
    return getSessionType(current_session) == SessionType::WORK;
}

bool PomodoroSequence::isBreakSession() const {
    SessionType type = getSessionType(current_session);
    return (type == SessionType::SHORT_BREAK || type == SessionType::LONG_BREAK);
}

bool PomodoroSequence::isLongBreak() const {
    return getSessionType(current_session) == SessionType::LONG_BREAK;
}

void PomodoroSequence::resetDailyCounter() {
    completed_today = 0;
}

uint32_t PomodoroSequence::serialize() const {
    // Pack into 32 bits:
    // [7:0]   = current_session (8 bits)
    // [15:8]  = completed_today (8 bits)
    // [31:16] = reserved (16 bits)
    uint32_t data = 0;
    data |= (current_session & 0xFF);
    data |= ((completed_today & 0xFF) << 8);
    return data;
}

void PomodoroSequence::deserialize(uint32_t data) {
    current_session = data & 0xFF;
    completed_today = (data >> 8) & 0xFF;

    // Validate and constrain
    if (current_session < 1 || current_session > getTotalIntervals()) {
        current_session = 1;
    }
}

// Private methods

PomodoroSequence::SessionType PomodoroSequence::getSessionType(uint8_t session_num) const {
    // Settings-based logic: uses only custom_sessions_before_long
    // Pattern repeats every cycle: W-B-W-B-...-W-LB
    // Example (4 sessions/cycle): W B W B W B W LB | W B W B W B W LB ...
    uint8_t intervals_per_cycle = custom_sessions_before_long * 2;

    // Long break at end of each cycle (sessions 4, 8, 12, etc.)
    if (session_num % intervals_per_cycle == 0) {
        return SessionType::LONG_BREAK;
    }

    // Short break on even intervals (but not long breaks)
    if (session_num % 2 == 0) {
        return SessionType::SHORT_BREAK;
    }

    // Work on odd intervals
    return SessionType::WORK;
}

uint16_t PomodoroSequence::getSessionDuration(SessionType type) const {
    // All modes now use custom durations (set via setWorkDuration(), etc.)
    // Defaults: CLASSIC (25/5/15), STUDY (45/15/15), or user-configured values
    switch (type) {
        case SessionType::WORK: return custom_work_min;
        case SessionType::SHORT_BREAK: return custom_short_break_min;
        case SessionType::LONG_BREAK: return custom_long_break_min;
    }

    return 25;  // Default fallback
}