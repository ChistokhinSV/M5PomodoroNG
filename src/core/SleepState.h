#ifndef SLEEP_STATE_H
#define SLEEP_STATE_H

#include <cstdint>
#include "TimerStateMachine.h"
#include "PomodoroSequence.h"
#include "../hardware/ILEDController.h"

/**
 * Sleep state preservation for deep sleep wake recovery
 *
 * ESP32 deep sleep resets the device, losing all RAM. This structure is stored
 * in RTC memory (8KB, survives deep sleep) to restore device state on wake.
 *
 * RTC_DATA_ATTR places variables in RTC_SLOW_MEM:
 * - Survives deep sleep (powered by RTC domain)
 * - Limited to 8KB total
 * - Cleared on power cycle or reset button
 * - NOT cleared on wake from deep sleep
 *
 * Usage:
 *   SleepState::save(state_machine, sequence, led_pattern);
 *   if (SleepState::restore(state_machine, sequence, led_controller)) {
 *       // Restored from deep sleep
 *   }
 */
class SleepState {
public:
    // Magic number to detect valid RTC data (0xCAFEBEEF)
    static constexpr uint32_t MAGIC = 0xCAFEBEEF;
    static constexpr uint8_t VERSION = 1;

    /**
     * Save current state to RTC memory before entering deep sleep
     *
     * @param state_machine Timer state machine to save
     * @param sequence Pomodoro sequence to save
     * @param led_pattern Current LED pattern (for restoration)
     */
    static void save(const TimerStateMachine& state_machine,
                     const PomodoroSequence& sequence,
                     ILEDController::TimerState led_pattern);

    /**
     * Restore state from RTC memory after waking from deep sleep
     *
     * @param state_machine Timer state machine to restore into
     * @param sequence Pomodoro sequence to restore into
     * @param led_controller LED controller to restore pattern
     * @return true if valid RTC data found and restored, false otherwise
     */
    static bool restore(TimerStateMachine& state_machine,
                        PomodoroSequence& sequence,
                        ILEDController* led_controller);

    /**
     * Clear RTC memory (invalidate saved state)
     */
    static void clear();

    /**
     * Check if valid RTC data exists
     */
    static bool isValid();

private:
    // RTC memory structure (must be POD - Plain Old Data)
    struct RTCData {
        uint32_t magic;                        // Magic number for validation
        uint8_t version;                       // Structure version for future migrations

        // Timer state machine
        uint8_t timer_state;                   // TimerStateMachine::State (IDLE=0, ACTIVE=1, PAUSED=2)
        uint32_t remaining_ms;                 // Remaining milliseconds (for PAUSED state)
        uint32_t total_ms;                     // Total session duration milliseconds

        // Pomodoro sequence
        uint8_t mode;                          // PomodoroMode (CLASSIC=0, STUDY=1, CUSTOM=2)
        uint8_t current_session_number;        // Session number in current cycle (1-based)
        uint8_t current_cycle;                 // Current cycle number (1-based)
        uint8_t session_type;                  // SessionType (WORK=0, SHORT_BREAK=1, LONG_BREAK=2)
        uint8_t completed_today;               // Completed work sessions today

        // LED pattern for restoration
        uint8_t led_pattern;                   // ILEDController::TimerState

        // Timestamp for debug/validation
        uint32_t save_timestamp_sec;           // Unix timestamp when saved
    };

    // RTC memory (survives deep sleep)
    static RTC_DATA_ATTR RTCData rtc_data_;
};

#endif // SLEEP_STATE_H
