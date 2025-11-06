#include "PomodoroSequence.h"
#include <Arduino.h>

PomodoroSequence::PomodoroSequence()
    : mode(Mode::CLASSIC),
      current_session(1),
      completed_today(0),
      sequence_start_epoch(0),
      custom_work_min(25),
      custom_short_break_min(5),
      custom_long_break_min(15),
      custom_sessions_before_long(4) {
}

void PomodoroSequence::setMode(Mode new_mode) {
    if (mode != new_mode) {
        mode = new_mode;
        reset();  // Reset sequence when mode changes
    }
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
    uint8_t total = getTotalSessions();

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

void PomodoroSequence::advance() {
    uint8_t total = getTotalSessions();
    current_session++;

    // Wrap around to 1 after completing full cycle
    if (current_session > total) {
        current_session = 1;
    }
}

uint8_t PomodoroSequence::getTotalSessions() const {
    switch (mode) {
        case Mode::CLASSIC:
            return 4;  // 4 work sessions (dots)
        case Mode::STUDY:
            return 1;  // 1 work session (dots)
        case Mode::CUSTOM:
            return custom_sessions_before_long;  // N work sessions (dots)
        default:
            return 4;
    }
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
    // [17:16] = mode (2 bits)
    // [31:18] = reserved (14 bits)
    uint32_t data = 0;
    data |= (current_session & 0xFF);
    data |= ((completed_today & 0xFF) << 8);
    data |= ((static_cast<uint8_t>(mode) & 0x03) << 16);
    return data;
}

void PomodoroSequence::deserialize(uint32_t data) {
    current_session = data & 0xFF;
    completed_today = (data >> 8) & 0xFF;
    mode = static_cast<Mode>((data >> 16) & 0x03);

    // Validate and constrain
    if (current_session < 1 || current_session > getTotalSessions()) {
        current_session = 1;
    }
}

// Private methods

PomodoroSequence::SessionType PomodoroSequence::getSessionType(uint8_t session_num) const {
    switch (mode) {
        case Mode::CLASSIC:
            // Pattern: W B W B W B W LB (1-8)
            if (session_num == 8) return SessionType::LONG_BREAK;
            if (session_num % 2 == 0) return SessionType::SHORT_BREAK;
            return SessionType::WORK;

        case Mode::STUDY:
            // Pattern: W B W B ... (alternating)
            return (session_num % 2 == 1) ? SessionType::WORK : SessionType::LONG_BREAK;

        case Mode::CUSTOM:
            // Similar to CLASSIC but with custom session count
            {
                uint8_t total_work_sessions = custom_sessions_before_long;
                uint8_t last_session = total_work_sessions * 2;

                if (session_num == last_session) return SessionType::LONG_BREAK;
                if (session_num % 2 == 0) return SessionType::SHORT_BREAK;
                return SessionType::WORK;
            }

        default:
            return SessionType::WORK;
    }
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