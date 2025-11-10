#ifndef TIMER_STATE_MACHINE_H
#define TIMER_STATE_MACHINE_H

#include "PomodoroSequence.h"
#include "../hardware/ILEDController.h"
#include "../hardware/IHapticController.h"
#include <cstdint>
#include <functional>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/**
 * Timer state machine implementing Pomodoro technique states and transitions
 *
 * Simplified 3-state design (merged RUNNING/BREAK into ACTIVE, eliminated COMPLETED)
 *
 * States:
 * - IDLE: No active timer, waiting for start
 * - ACTIVE: Timer counting down (work or break session)
 * - PAUSED: Timer paused (can resume)
 *
 * Events:
 * - START: Begin timer for current session
 * - PAUSE: Pause timer
 * - RESUME: Resume from pause
 * - STOP: Stop and reset timer
 * - TIMEOUT: Timer reached zero (auto-advances sequence)
 * - SKIP: Skip current session and advance
 *
 * Thread-Safety (MP-47):
 * - All public methods are protected by internal mutex
 * - Safe to call from any task (Core 0 or Core 1)
 * - Timeout: 50ms (state operations are fast)
 * - Uses RAII MutexGuard for automatic mutex release
 */
class TimerStateMachine {
public:
    enum class State {
        IDLE,
        ACTIVE,
        PAUSED
    };

    enum class Event {
        START,
        PAUSE,
        RESUME,
        STOP,
        TIMEOUT,
        SKIP
    };

    // Callback types for state transitions
    using StateCallback = std::function<void(State old_state, State new_state)>;
    using TimeoutCallback = std::function<void()>;
    using AudioCallback = std::function<void(const char* sound_name)>;

    TimerStateMachine(PomodoroSequence& sequence);
    ~TimerStateMachine();

    // State machine control (thread-safe)
    bool handleEvent(Event event);
    State getState() const { return state; }
    const char* getStateName() const;

    // Timer control
    void update(uint32_t delta_ms);  // Call every loop iteration
    uint32_t getRemainingMs() const { return remaining_ms; }
    uint32_t getTotalMs() const { return total_ms; }
    uint8_t getProgressPercent() const;

    // Time formatting helpers
    void getRemainingTime(uint8_t& minutes, uint8_t& seconds) const;
    bool isActive() const { return state == State::ACTIVE; }

    // Callbacks
    void onStateChange(StateCallback callback) { state_callback = callback; }
    void onTimeout(TimeoutCallback callback) { timeout_callback = callback; }
    void onAudioEvent(AudioCallback callback) { audio_callback = callback; }

    // LED controller (MP-23)
    void setLEDController(ILEDController* controller) { led_controller = controller; }
    void indicateSessionReady();  // Show yellow flash when waiting for session start

    // Haptic controller (MP-27)
    void setHapticController(IHapticController* controller) { haptic_controller = controller; }

    // Reset to IDLE
    void reset();

private:
    PomodoroSequence& sequence;
    State state = State::IDLE;
    uint32_t remaining_ms = 0;
    uint32_t total_ms = 0;
    uint32_t last_warning_check_ms = 0;  // Track when we last checked for 30s warning
    bool warning_played = false;  // Track if warning already played for current session
    StateCallback state_callback = nullptr;
    TimeoutCallback timeout_callback = nullptr;
    AudioCallback audio_callback = nullptr;
    ILEDController* led_controller = nullptr;  // MP-23: LED control
    IHapticController* haptic_controller = nullptr;  // MP-27: Haptic feedback

    // Thread-safety (MP-47)
    SemaphoreHandle_t state_mutex_;  // Protects all state variables above

    // State transition logic
    bool transition(State new_state);
    bool canTransition(Event event) const;
    void enterState(State new_state);
    void exitState(State old_state);

    // Timer management
    void startTimer(uint16_t duration_min);
    void pauseTimer();
    void resumeTimer();
    void stopTimer();
};

#endif // TIMER_STATE_MACHINE_H