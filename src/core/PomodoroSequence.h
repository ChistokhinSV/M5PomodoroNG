#ifndef POMODORO_SEQUENCE_H
#define POMODORO_SEQUENCE_H

#include <cstdint>

/**
 * Pomodoro sequence tracking and logic
 *
 * Manages configurable work/break sequences:
 * - Work session → Short break → repeat (N-1) times
 * - Work session → Long break → reset cycle
 *
 * All behavior is driven by numeric settings:
 * - Work/break durations (custom_work_min, custom_short_break_min, custom_long_break_min)
 * - Sessions per cycle (custom_sessions_before_long)
 * - Number of cycles (custom_num_cycles)
 *
 * Example configurations:
 * - Classic: 25/5/15 min, 4 sessions, 1 cycle
 * - Study: 45/15/30 min, 2 sessions, 2 cycles
 * - Custom: any duration/session/cycle combination
 */
class PomodoroSequence {
public:
    enum class SessionType {
        WORK,
        SHORT_BREAK,
        LONG_BREAK
    };

    struct Session {
        SessionType type;
        uint8_t number;          // 1-based interval number
        uint16_t duration_min;
    };

    PomodoroSequence();

    // Configuration (durations and cycle settings)
    void setWorkDuration(uint16_t minutes);
    void setShortBreakDuration(uint16_t minutes);
    void setLongBreakDuration(uint16_t minutes);
    void setSessionsBeforeLong(uint8_t count);
    void setNumCycles(uint8_t cycles);

    // Sequence control
    void start();                // Start new sequence
    void reset();                // Reset to session 1
    Session getCurrentSession() const;
    Session getNextSession() const;
    bool advance();              // Move to next session, returns true if cycle completed

    // Query methods (work session based)
    uint8_t getTotalWorkSessions() const;      // Total work sessions (sessions_before_long × num_cycles)
    uint8_t getCurrentWorkSession() const;      // Current work session number (1-N)
    uint8_t getSessionsBeforeLong() const;     // Sessions per cycle (custom_sessions_before_long)
    uint8_t getCompletedToday() const { return completed_today; }
    bool isWorkSession() const;
    bool isBreakSession() const;
    bool isLongBreak() const;
    bool isNextLongBreak() const;              // True if next interval is long break

    // Internal methods (for compatibility during refactor)
    uint8_t getCurrentSessionNumber() const { return current_session; }  // Returns interval number
    uint8_t getTotalIntervals() const;         // Total intervals (sessions_before_long × 2 × num_cycles)
    uint8_t getTotalSessions() const;          // Legacy wrapper for getTotalIntervals()

    // Daily reset (call at midnight)
    void resetDailyCounter();
    void incrementCompletedToday() { completed_today++; }

    // Serialization for NVS storage
    uint32_t serialize() const;
    void deserialize(uint32_t data);

private:
    uint8_t current_session = 1;     // 1-based session number
    uint8_t completed_today = 0;     // Daily counter
    uint32_t sequence_start_epoch = 0;

    // Session configuration (durations, cycle settings)
    uint16_t custom_work_min = 25;
    uint16_t custom_short_break_min = 5;
    uint16_t custom_long_break_min = 15;
    uint8_t custom_sessions_before_long = 4;
    uint8_t custom_num_cycles = 1;

    SessionType getSessionType(uint8_t session_num) const;
    uint16_t getSessionDuration(SessionType type) const;
};

#endif // POMODORO_SEQUENCE_H
