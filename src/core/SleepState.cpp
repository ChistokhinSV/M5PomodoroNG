#include "SleepState.h"
#include <Arduino.h>
#include <sys/time.h>

// Initialize RTC memory (zeroed on first power-up)
RTC_DATA_ATTR SleepState::RTCData SleepState::rtc_data_ = {0};

void SleepState::save(const TimerStateMachine& state_machine,
                      const PomodoroSequence& sequence,
                      ILEDController::TimerState led_pattern) {
    Serial.println("[SleepState] Saving state to RTC memory...");

    // Mark data as valid
    rtc_data_.magic = MAGIC;
    rtc_data_.version = VERSION;

    // Save timer state machine
    rtc_data_.timer_state = static_cast<uint8_t>(state_machine.getState());
    rtc_data_.remaining_ms = state_machine.getRemainingMs();
    rtc_data_.total_ms = state_machine.getTotalMs();

    // Save pomodoro sequence
    rtc_data_.mode = 0;  // Mode not implemented yet (MP-50), default to classic
    auto session = sequence.getCurrentSession();
    rtc_data_.current_session_number = session.number;
    rtc_data_.current_cycle = sequence.getCurrentCycle();
    rtc_data_.session_type = static_cast<uint8_t>(session.type);
    rtc_data_.completed_today = sequence.getCompletedToday();

    // Save LED pattern
    rtc_data_.led_pattern = static_cast<uint8_t>(led_pattern);

    // Save timestamp (for debug/validation)
    struct timeval tv;
    gettimeofday(&tv, NULL);
    rtc_data_.save_timestamp_sec = tv.tv_sec;

    Serial.printf("[SleepState] Saved: state=%d, remaining=%lums, session=%d, mode=%d, completed=%d\n",
                  rtc_data_.timer_state, rtc_data_.remaining_ms,
                  rtc_data_.current_session_number, rtc_data_.mode,
                  rtc_data_.completed_today);
}

bool SleepState::restore(TimerStateMachine& state_machine,
                         PomodoroSequence& sequence,
                         ILEDController* led_controller) {
    // Check if RTC data is valid
    if (!isValid()) {
        Serial.println("[SleepState] No valid RTC data found");
        return false;
    }

    Serial.println("[SleepState] Restoring state from RTC memory...");
    Serial.printf("[SleepState] Saved: state=%d, remaining=%lums, session=%d, mode=%d, completed=%d\n",
                  rtc_data_.timer_state, rtc_data_.remaining_ms,
                  rtc_data_.current_session_number, rtc_data_.mode,
                  rtc_data_.completed_today);

    // Restore pomodoro sequence first (timer depends on it)
    // Note: Mode restoration requires PomodoroMode enum (not implemented yet in MP-50)
    // For now, assume classic mode (default)

    // Restore completed count
    sequence.setCompletedToday(rtc_data_.completed_today);

    // Restore session position directly (no need to advance() repeatedly)
    sequence.setCurrentSession(rtc_data_.current_session_number);

    // Restore timer state machine
    auto saved_state = static_cast<TimerStateMachine::State>(rtc_data_.timer_state);

    if (saved_state == TimerStateMachine::State::IDLE) {
        // Just return to IDLE (may be in "session ready" yellow flash mode)
        state_machine.reset();
        Serial.println("[SleepState] Restored to IDLE state");
    } else if (saved_state == TimerStateMachine::State::PAUSED) {
        // Restore PAUSED state with exact remaining time using restoreState() method
        state_machine.restoreState(TimerStateMachine::State::PAUSED,
                                    rtc_data_.remaining_ms,
                                    rtc_data_.total_ms);
        Serial.println("[SleepState] Restored to PAUSED state with exact time");
    } else if (saved_state == TimerStateMachine::State::ACTIVE) {
        // Device was ACTIVE when sleeping - unusual but possible
        // Return to IDLE instead (don't auto-resume timer)
        state_machine.reset();
        Serial.println("[SleepState] Was ACTIVE, returning to IDLE");
    }

    // Restore LED pattern
    if (led_controller) {
        auto pattern = static_cast<ILEDController::TimerState>(rtc_data_.led_pattern);
        led_controller->setStatePattern(pattern);
        Serial.printf("[SleepState] Restored LED pattern: %d\n", rtc_data_.led_pattern);
    }

    // Log wake timestamp
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint32_t sleep_duration_sec = tv.tv_sec - rtc_data_.save_timestamp_sec;
    Serial.printf("[SleepState] Slept for %lu seconds\n", sleep_duration_sec);

    return true;
}

void SleepState::clear() {
    Serial.println("[SleepState] Clearing RTC memory");
    rtc_data_.magic = 0;
    rtc_data_.version = 0;
}

bool SleepState::isValid() {
    return (rtc_data_.magic == MAGIC && rtc_data_.version == VERSION);
}
