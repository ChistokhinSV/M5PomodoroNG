#ifndef POMODORO_SEQUENCE_H
#define POMODORO_SEQUENCE_H

#include <cstdint>

/**
 * Pomodoro sequence tracking and logic
 *
 * Manages the classic Pomodoro technique sequence:
 * - Work session (25min) → Short break (5min) → repeat 3 times
 * - Work session (25min) → Long break (15min) → reset sequence
 *
 * Also supports:
 * - STUDY mode: 45min work → 15min break (repeating)
 * - CUSTOM mode: User-defined durations
 */
class PomodoroSequence {
public:
    enum class Mode {
        CLASSIC,    // 25-5-25-5-25-5-25-15 pattern
        STUDY,      // 45-15-45-15... pattern
        CUSTOM      // User-defined durations
    };

    enum class SessionType {
        WORK,
        SHORT_BREAK,
        LONG_BREAK
    };

    struct Session {
        SessionType type;
        uint8_t number;          // 1-4 for CLASSIC mode
        uint16_t duration_min;
    };

    PomodoroSequence();

    // Mode management
    void setMode(Mode mode);
    Mode getMode() const { return mode; }

    // Custom durations (for CUSTOM mode)
    void setWorkDuration(uint16_t minutes);
    void setShortBreakDuration(uint16_t minutes);
    void setLongBreakDuration(uint16_t minutes);
    void setSessionsBeforeLong(uint8_t count);

    // Sequence control
    void start();                // Start new sequence
    void reset();                // Reset to session 1
    Session getCurrentSession() const;
    Session getNextSession() const;
    void advance();              // Move to next session

    // Query methods
    uint8_t getCurrentSessionNumber() const { return current_session; }
    uint8_t getTotalSessions() const;
    uint8_t getCompletedToday() const { return completed_today; }
    bool isWorkSession() const;
    bool isBreakSession() const;
    bool isLongBreak() const;

    // Daily reset (call at midnight)
    void resetDailyCounter();
    void incrementCompletedToday() { completed_today++; }

    // Serialization for NVS storage
    uint32_t serialize() const;
    void deserialize(uint32_t data);

private:
    Mode mode = Mode::CLASSIC;
    uint8_t current_session = 1;     // 1-based session number
    uint8_t completed_today = 0;     // Daily counter
    uint32_t sequence_start_epoch = 0;

    // Custom durations (CUSTOM mode)
    uint16_t custom_work_min = 25;
    uint16_t custom_short_break_min = 5;
    uint16_t custom_long_break_min = 15;
    uint8_t custom_sessions_before_long = 4;

    SessionType getSessionType(uint8_t session_num) const;
    uint16_t getSessionDuration(SessionType type) const;
};

#endif // POMODORO_SEQUENCE_H
