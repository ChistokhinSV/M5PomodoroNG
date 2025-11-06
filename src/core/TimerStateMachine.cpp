#include "TimerStateMachine.h"
#include "../utils/MutexGuard.h"
#include <Arduino.h>

TimerStateMachine::TimerStateMachine(PomodoroSequence& seq)
    : sequence(seq),
      state(State::IDLE),
      remaining_ms(0),
      total_ms(0),
      state_mutex_(xSemaphoreCreateMutex()) {

    if (state_mutex_ == NULL) {
        Serial.println("[TimerStateMachine] ERROR: Failed to create state mutex");
    } else {
        Serial.println("[TimerStateMachine] Mutex created (thread-safe)");
    }
}

TimerStateMachine::~TimerStateMachine() {
    if (state_mutex_ != NULL) {
        vSemaphoreDelete(state_mutex_);
        state_mutex_ = NULL;
    }
}

bool TimerStateMachine::handleEvent(Event event) {
    MutexGuard guard(state_mutex_, "state_mutex", 50);
    if (!guard.isLocked()) {
        Serial.println("[TimerStateMachine] ERROR: Failed to acquire mutex in handleEvent");
        return false;
    }

    if (!canTransition(event)) {
        Serial.printf("[StateMachine] Invalid event %d in state %d\n",
                      static_cast<int>(event), static_cast<int>(state));
        return false;
    }

    State old_state = state;

    switch (event) {
        case Event::START:
            if (state == State::IDLE) {
                auto session = sequence.getCurrentSession();
                startTimer(session.duration_min);
                return transition(State::ACTIVE);
            }
            break;

        case Event::PAUSE:
            if (state == State::ACTIVE) {
                pauseTimer();
                return transition(State::PAUSED);
            }
            break;

        case Event::RESUME:
            if (state == State::PAUSED) {
                resumeTimer();
                return transition(State::ACTIVE);
            }
            break;

        case Event::STOP:
            stopTimer();
            return transition(State::IDLE);

        case Event::TIMEOUT:
            if (state == State::ACTIVE) {
                // Increment completed count for work sessions
                if (sequence.isWorkSession()) {
                    sequence.incrementCompletedToday();
                }

                // Auto-advance to next session
                sequence.advance();

                // Return to IDLE
                stopTimer();
                bool transitioned = transition(State::IDLE);

                // Trigger timeout callback AFTER transitioning to IDLE
                // This allows the callback to check the new session and auto-start if configured
                if (transitioned && timeout_callback) {
                    timeout_callback();
                }

                return transitioned;
            }
            break;

        case Event::SKIP:
            // Skip current session and advance to next
            sequence.advance();
            stopTimer();
            return transition(State::IDLE);
    }

    return false;
}

void TimerStateMachine::update(uint32_t delta_ms) {
    MutexGuard guard(state_mutex_, "state_mutex", 50);
    if (!guard.isLocked()) {
        Serial.println("[TimerStateMachine] ERROR: Failed to acquire mutex in update");
        return;
    }

    if (state != State::ACTIVE) {
        return;  // Timer not active
    }

    if (remaining_ms == 0) {
        return;  // Already at zero
    }

    // Check for 30-second warning (only once per session)
    if (!warning_played && remaining_ms <= 30000 && remaining_ms > 29000) {
        if (audio_callback) {
            audio_callback("warning");
        }
        warning_played = true;
    }

    if (delta_ms >= remaining_ms) {
        remaining_ms = 0;
        // Note: handleEvent will acquire mutex again, but that's OK because we're using RAII guard
        // The guard will release before handleEvent is called
        guard.unlock();  // Explicit unlock before recursive call
        handleEvent(Event::TIMEOUT);
    } else {
        remaining_ms -= delta_ms;
    }
}

uint8_t TimerStateMachine::getProgressPercent() const {
    // Note: Need to cast away const to use MutexGuard with const method
    // This is safe because we're only reading, not modifying
    TimerStateMachine* self = const_cast<TimerStateMachine*>(this);
    MutexGuard guard(self->state_mutex_, "state_mutex", 50);
    if (!guard.isLocked()) {
        return 0;  // Return safe value on mutex timeout
    }

    if (total_ms == 0) return 0;

    uint32_t elapsed = total_ms - remaining_ms;
    return (elapsed * 100) / total_ms;
}

void TimerStateMachine::getRemainingTime(uint8_t& minutes, uint8_t& seconds) const {
    TimerStateMachine* self = const_cast<TimerStateMachine*>(this);
    MutexGuard guard(self->state_mutex_, "state_mutex", 50);
    if (!guard.isLocked()) {
        minutes = 0;
        seconds = 0;
        return;
    }

    uint32_t total_seconds = remaining_ms / 1000;
    minutes = total_seconds / 60;
    seconds = total_seconds % 60;
}

const char* TimerStateMachine::getStateName() const {
    TimerStateMachine* self = const_cast<TimerStateMachine*>(this);
    MutexGuard guard(self->state_mutex_, "state_mutex", 50);
    if (!guard.isLocked()) {
        return "UNKNOWN";
    }

    switch (state) {
        case State::IDLE: return "IDLE";
        case State::ACTIVE: return "ACTIVE";
        case State::PAUSED: return "PAUSED";
        default: return "UNKNOWN";
    }
}

void TimerStateMachine::reset() {
    MutexGuard guard(state_mutex_, "state_mutex", 50);
    if (!guard.isLocked()) {
        Serial.println("[TimerStateMachine] ERROR: Failed to acquire mutex in reset");
        return;
    }

    stopTimer();
    transition(State::IDLE);
}

// Private methods

bool TimerStateMachine::transition(State new_state) {
    if (state == new_state) {
        return false;  // No transition needed
    }

    State old_state = state;

    // Store old state name BEFORE transition
    const char* old_name = (old_state == State::IDLE) ? "IDLE" :
                           (old_state == State::ACTIVE) ? "ACTIVE" :
                           (old_state == State::PAUSED) ? "PAUSED" : "UNKNOWN";

    exitState(old_state);
    state = new_state;
    enterState(new_state);

    Serial.printf("[StateMachine] Transition: %s -> %s\n",
                  old_name, getStateName());

    if (state_callback) {
        state_callback(old_state, new_state);
    }

    return true;
}

bool TimerStateMachine::canTransition(Event event) const {
    switch (state) {
        case State::IDLE:
            return event == Event::START;

        case State::ACTIVE:
            return event == Event::PAUSE ||
                   event == Event::STOP ||
                   event == Event::TIMEOUT ||
                   event == Event::SKIP;

        case State::PAUSED:
            return event == Event::RESUME ||
                   event == Event::STOP ||
                   event == Event::SKIP;

        default:
            return false;
    }
}

void TimerStateMachine::enterState(State new_state) {
    switch (new_state) {
        case State::IDLE:
            remaining_ms = 0;
            total_ms = 0;
            break;

        case State::ACTIVE:
            // Timer already set by startTimer()

            // Reset warning flag for new session
            warning_played = false;

            // Trigger audio based on session type
            if (audio_callback) {
                auto session_type = sequence.getCurrentSession().type;
                if (session_type == PomodoroSequence::SessionType::WORK) {
                    audio_callback("work_start");
                } else if (session_type == PomodoroSequence::SessionType::SHORT_BREAK) {
                    audio_callback("rest_start");
                } else if (session_type == PomodoroSequence::SessionType::LONG_BREAK) {
                    audio_callback("long_rest_start");
                }
            }
            break;

        case State::PAUSED:
            // Keep remaining_ms intact
            break;
    }
}

void TimerStateMachine::exitState(State old_state) {
    // Cleanup actions when leaving a state
    // Currently no state-specific cleanup needed
    (void)old_state;  // Unused parameter
}

void TimerStateMachine::startTimer(uint16_t duration_min) {
    total_ms = duration_min * 60 * 1000;
    remaining_ms = total_ms;

    Serial.printf("[StateMachine] Timer started: %u minutes (%lu ms)\n",
                  duration_min, total_ms);
}

void TimerStateMachine::pauseTimer() {
    // remaining_ms stays as-is
    Serial.printf("[StateMachine] Timer paused: %lu ms remaining\n", remaining_ms);
}

void TimerStateMachine::resumeTimer() {
    // remaining_ms resumes from paused value
    Serial.printf("[StateMachine] Timer resumed: %lu ms remaining\n", remaining_ms);
}

void TimerStateMachine::stopTimer() {
    remaining_ms = 0;
    total_ms = 0;
    Serial.println("[StateMachine] Timer stopped");
}