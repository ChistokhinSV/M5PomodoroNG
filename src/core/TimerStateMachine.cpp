#include "TimerStateMachine.h"
#include <Arduino.h>

TimerStateMachine::TimerStateMachine(PomodoroSequence& seq)
    : sequence(seq),
      state(State::IDLE),
      remaining_ms(0),
      total_ms(0) {
}

bool TimerStateMachine::handleEvent(Event event) {
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

                if (session.type == PomodoroSequence::SessionType::WORK) {
                    return transition(State::RUNNING);
                } else {
                    return transition(State::BREAK);
                }
            }
            break;

        case Event::PAUSE:
            if (state == State::RUNNING || state == State::BREAK) {
                pauseTimer();
                return transition(State::PAUSED);
            }
            break;

        case Event::RESUME:
            if (state == State::PAUSED) {
                resumeTimer();

                // Resume to previous state (RUNNING or BREAK)
                auto session = sequence.getCurrentSession();
                if (session.type == PomodoroSequence::SessionType::WORK) {
                    return transition(State::RUNNING);
                } else {
                    return transition(State::BREAK);
                }
            }
            break;

        case Event::STOP:
            stopTimer();
            return transition(State::IDLE);

        case Event::TIMEOUT:
            if (state == State::RUNNING || state == State::BREAK) {
                // Timer reached zero
                if (timeout_callback) {
                    timeout_callback();
                }
                return transition(State::COMPLETED);
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
    if (state != State::RUNNING && state != State::BREAK) {
        return;  // Timer not active
    }

    if (remaining_ms == 0) {
        return;  // Already at zero
    }

    if (delta_ms >= remaining_ms) {
        remaining_ms = 0;
        handleEvent(Event::TIMEOUT);
    } else {
        remaining_ms -= delta_ms;
    }
}

uint8_t TimerStateMachine::getProgressPercent() const {
    if (total_ms == 0) return 0;

    uint32_t elapsed = total_ms - remaining_ms;
    return (elapsed * 100) / total_ms;
}

void TimerStateMachine::getRemainingTime(uint8_t& minutes, uint8_t& seconds) const {
    uint32_t total_seconds = remaining_ms / 1000;
    minutes = total_seconds / 60;
    seconds = total_seconds % 60;
}

const char* TimerStateMachine::getStateName() const {
    switch (state) {
        case State::IDLE: return "IDLE";
        case State::RUNNING: return "RUNNING";
        case State::PAUSED: return "PAUSED";
        case State::BREAK: return "BREAK";
        case State::COMPLETED: return "COMPLETED";
        default: return "UNKNOWN";
    }
}

void TimerStateMachine::reset() {
    stopTimer();
    transition(State::IDLE);
}

// Private methods

bool TimerStateMachine::transition(State new_state) {
    if (state == new_state) {
        return false;  // No transition needed
    }

    State old_state = state;

    exitState(old_state);
    state = new_state;
    enterState(new_state);

    Serial.printf("[StateMachine] Transition: %s -> %s\n",
                  getStateName(), getStateName());

    if (state_callback) {
        state_callback(old_state, new_state);
    }

    return true;
}

bool TimerStateMachine::canTransition(Event event) const {
    switch (state) {
        case State::IDLE:
            return event == Event::START;

        case State::RUNNING:
            return event == Event::PAUSE ||
                   event == Event::STOP ||
                   event == Event::TIMEOUT ||
                   event == Event::SKIP;

        case State::PAUSED:
            return event == Event::RESUME ||
                   event == Event::STOP ||
                   event == Event::SKIP;

        case State::BREAK:
            return event == Event::PAUSE ||
                   event == Event::STOP ||
                   event == Event::TIMEOUT ||
                   event == Event::SKIP;

        case State::COMPLETED:
            return event == Event::START ||
                   event == Event::STOP;

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

        case State::RUNNING:
        case State::BREAK:
            // Timer already set by startTimer()
            break;

        case State::PAUSED:
            // Keep remaining_ms intact
            break;

        case State::COMPLETED:
            remaining_ms = 0;
            // Increment completed count for work sessions
            if (sequence.isWorkSession()) {
                sequence.incrementCompletedToday();
            }
            // Auto-advance to next session
            sequence.advance();
            break;
    }
}

void TimerStateMachine::exitState(State old_state) {
    // Cleanup actions when leaving a state
    switch (old_state) {
        case State::RUNNING:
        case State::BREAK:
            // Nothing specific to clean up
            break;

        case State::PAUSED:
            // Nothing specific to clean up
            break;

        default:
            break;
    }
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