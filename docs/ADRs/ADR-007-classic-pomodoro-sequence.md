# ADR-007: Classic Pomodoro Sequence Implementation

**Status**: Accepted

**Date**: 2025-12-15

**Decision Makers**: Development Team

## Context

The Pomodoro Technique defines a specific sequence of work and rest intervals to maximize productivity. We need to decide which Pomodoro variant(s) to implement and how to track session progression.

### Classic Pomodoro Technique (Original)

```
Session 1:  25min work → 5min rest
Session 2:  25min work → 5min rest
Session 3:  25min work → 5min rest
Session 4:  25min work → 15min rest (long rest)
→ Repeat cycle
```

**Total cycle time**: 130 minutes (2h 10m)
**Sessions per day** (8-hour workday): ~3-4 cycles = 12-16 Pomodoros

### Common Variants

1. **Study Mode** (45-15): 45min work → 15min rest, no long rest
2. **Sprint Mode** (15-3): 15min work → 3min rest
3. **Custom**: User-defined durations

### Requirements

1. Support classic 4-session cycle (25-5-25-5-25-5-25-15)
2. Support Study mode (45-15) as alternative
3. Track session number and sequence position
4. Persist sequence state across reboots
5. Auto-advance to next session on timer expiry
6. Allow manual session reset

## Decision

**We will implement two modes:**

1. **Classic Mode** (default): 4-session cycle with long rest after session 4
   - Pomodoro duration: 25 minutes (configurable 15-60min)
   - Short rest: 5 minutes (configurable 3-15min)
   - Long rest: 15 minutes (configurable 10-30min)
   - Sequence: P → SR → P → SR → P → SR → P → LR → repeat

2. **Study Mode**: Simple alternating work/rest, no long rest
   - Study duration: 45 minutes (configurable 30-90min)
   - Rest duration: 15 minutes (configurable 10-30min)
   - Sequence: S → R → S → R → repeat

**No Sprint Mode or custom sequences in v2.0** (defer to future version).

### Rationale

1. **Adherence to Methodology**: Classic Pomodoro is scientifically validated for productivity
2. **Long Rest Importance**: 15-minute long rest prevents burnout during long work sessions
3. **Study Mode Demand**: Commonly requested by students for longer focus periods
4. **Simplicity**: Two modes cover 90% of use cases without UI complexity
5. **Future Extensibility**: Architecture supports adding more modes in future versions

## Implementation

### State Machine Extension

```cpp
enum class PomodoroMode {
    CLASSIC,      // 25-5-25-5-25-5-25-15
    STUDY         // 45-15-45-15-...
};

enum class PomodoroState {
    STOPPED,
    POMODORO,     // Work session (25min or 45min)
    SHORT_REST,   // Short rest (5min)
    LONG_REST,    // Long rest (15min, classic mode only)
    PAUSED
};

class TimerStateMachine {
private:
    PomodoroMode currentMode;
    PomodoroState currentState;
    uint8_t sessionNumber;        // 1-4 in classic, 1+ in study
    uint8_t sequencePosition;     // 0-7 in classic (8 stages), 0-1 in study
    uint16_t remainingSeconds;
    Settings settings;

public:
    void setMode(PomodoroMode mode);
    void handleEvent(TimerEvent event);
    void tick();  // Called every second
    PomodoroState getNextState();
    uint16_t getDuration(PomodoroState state);
};
```

### Sequence Logic (Classic Mode)

```cpp
PomodoroState TimerStateMachine::getNextState() {
    if (currentMode == PomodoroMode::CLASSIC) {
        // Classic sequence: P → SR → P → SR → P → SR → P → LR → repeat
        switch (sequencePosition) {
            case 0: return PomodoroState::POMODORO;      // Session 1
            case 1: return PomodoroState::SHORT_REST;
            case 2: return PomodoroState::POMODORO;      // Session 2
            case 3: return PomodoroState::SHORT_REST;
            case 4: return PomodoroState::POMODORO;      // Session 3
            case 5: return PomodoroState::SHORT_REST;
            case 6: return PomodoroState::POMODORO;      // Session 4
            case 7: return PomodoroState::LONG_REST;
            default:
                sequencePosition = 0;  // Reset cycle
                return PomodoroState::POMODORO;
        }
    } else if (currentMode == PomodoroMode::STUDY) {
        // Study sequence: S → R → S → R → ...
        if (sequencePosition % 2 == 0) {
            return PomodoroState::POMODORO;  // Study session
        } else {
            return PomodoroState::SHORT_REST;  // Rest
        }
    }

    return PomodoroState::STOPPED;
}

uint16_t TimerStateMachine::getDuration(PomodoroState state) {
    switch (state) {
        case PomodoroState::POMODORO:
            if (currentMode == PomodoroMode::CLASSIC) {
                return settings.pomodoroDuration * 60;  // 25 min default
            } else {
                return settings.studyDuration * 60;     // 45 min default
            }

        case PomodoroState::SHORT_REST:
            if (currentMode == PomodoroMode::CLASSIC) {
                return settings.shortRestDuration * 60;  // 5 min
            } else {
                return settings.studyRestDuration * 60;  // 15 min
            }

        case PomodoroState::LONG_REST:
            return settings.longRestDuration * 60;  // 15 min (classic only)

        default:
            return 0;
    }
}
```

### Timer Expiry and Auto-Advance

```cpp
void TimerStateMachine::handleEvent(TimerEvent event) {
    switch (event) {
        case TimerEvent::EXPIRE:
            // Current session expired, auto-advance to next
            LOG_INFO("Session expired: " + stateToString(currentState));

            // Update statistics
            if (currentState == PomodoroState::POMODORO) {
                statistics.recordSessionComplete(sessionNumber);
            }

            // Advance sequence
            sequencePosition++;
            if (currentMode == PomodoroMode::CLASSIC && sequencePosition >= 8) {
                sequencePosition = 0;  // Reset cycle
                sessionNumber = 1;
            } else if (currentState == PomodoroState::POMODORO) {
                sessionNumber++;
            }

            // Transition to next state
            currentState = getNextState();
            remainingSeconds = getDuration(currentState);

            // Trigger feedback
            if (currentState == PomodoroState::POMODORO) {
                playSound("ready_for_work.wav");
                setLEDPattern(LEDPattern::FOCUS);
            } else {
                playSound("rest_start.wav");
                setLEDPattern(LEDPattern::REST);
            }

            // Auto-start next session (configurable)
            if (settings.autoStartNextSession) {
                LOG_INFO("Auto-starting next session: " + stateToString(currentState));
            } else {
                currentState = PomodoroState::STOPPED;
                LOG_INFO("Session complete, waiting for user to start next");
            }

            // Sync to cloud
            cloudSync.publishSessionComplete();
            break;

        // ... other events
    }
}
```

### Persistence Across Reboots

```cpp
// Save sequence state to NVS on pause/stop
void TimerStateMachine::saveState() {
    nvs_handle_t handle;
    nvs_open("timer_state", NVS_READWRITE, &handle);

    nvs_set_u8(handle, "mode", static_cast<uint8_t>(currentMode));
    nvs_set_u8(handle, "state", static_cast<uint8_t>(currentState));
    nvs_set_u8(handle, "session", sessionNumber);
    nvs_set_u8(handle, "sequence", sequencePosition);
    nvs_set_u16(handle, "remaining", remainingSeconds);
    nvs_set_u32(handle, "started_at", sessionStartEpoch);

    nvs_commit(handle);
    nvs_close(handle);

    LOG_INFO("Timer state saved to NVS");
}

// Restore state on boot
void TimerStateMachine::restoreState() {
    nvs_handle_t handle;
    if (nvs_open("timer_state", NVS_READONLY, &handle) == ESP_OK) {
        uint8_t mode, state, session, sequence;
        uint16_t remaining;
        uint32_t started_at;

        if (nvs_get_u8(handle, "mode", &mode) == ESP_OK) {
            currentMode = static_cast<PomodoroMode>(mode);
        }
        if (nvs_get_u8(handle, "state", &state) == ESP_OK) {
            currentState = static_cast<PomodoroState>(state);
        }
        if (nvs_get_u8(handle, "session", &session) == ESP_OK) {
            sessionNumber = session;
        }
        if (nvs_get_u8(handle, "sequence", &sequence) == ESP_OK) {
            sequencePosition = sequence;
        }
        if (nvs_get_u16(handle, "remaining", &remaining) == ESP_OK) {
            remainingSeconds = remaining;
        }
        if (nvs_get_u32(handle, "started_at", &started_at) == ESP_OK) {
            sessionStartEpoch = started_at;
        }

        nvs_close(handle);

        // Verify state is still valid
        if (currentState == PomodoroState::PAUSED) {
            LOG_INFO("Restored paused timer: " + String(remainingSeconds) + "s remaining");
        } else if (currentState != PomodoroState::STOPPED) {
            // Timer was running during power loss, calculate elapsed time
            uint32_t now = getCurrentEpoch();
            uint32_t elapsed = now - sessionStartEpoch;

            if (elapsed < remainingSeconds) {
                remainingSeconds -= elapsed;
                LOG_INFO("Restored running timer: " + String(remainingSeconds) + "s remaining");
            } else {
                // Timer expired during power loss
                LOG_WARN("Timer expired during power loss, resetting");
                currentState = PomodoroState::STOPPED;
            }
        }
    }
}
```

### UI Sequence Indicator

```cpp
// Display sequence progress (4 groups of 4 dots for classic mode)
void SequenceIndicator::render(int16_t x, int16_t y, uint8_t currentPos, uint8_t completedPos) {
    if (currentMode == PomodoroMode::CLASSIC) {
        // Classic: 4 groups of 4 dots
        // [●●●●] [○○○○] [○○○○] [○○○○]
        //  ^-- Group 1 (sessions 1-2)

        for (uint8_t group = 0; group < 4; group++) {
            for (uint8_t dot = 0; dot < 4; dot++) {
                uint8_t pos = group * 4 + dot;

                bool isCompleted = (pos < completedPos);
                bool isCurrent = (pos == currentPos);

                if (isCompleted) {
                    M5.Display.fillCircle(x, y, 6, COLOR_SUCCESS);  // Green
                } else if (isCurrent) {
                    M5.Display.fillCircle(x, y, 6, COLOR_PRIMARY);  // Red (pulsing)
                } else {
                    M5.Display.drawCircle(x, y, 6, COLOR_TEXT_DISABLED);  // Gray
                }

                x += 16;  // Spacing between dots
            }
            x += 8;  // Extra spacing between groups
        }
    } else if (currentMode == PomodoroMode::STUDY) {
        // Study: Simple counter
        M5.Display.setFont(&fonts::Font4);
        M5.Display.setTextColor(COLOR_TEXT_PRIMARY);
        M5.Display.drawString("Session " + String(sessionNumber), x, y);
    }
}
```

## Sequence Transition Examples

### Classic Mode (Full Cycle)

```
Session 1 (sequencePos=0):
├─ State: POMODORO (25:00)
├─ User rotates device → Timer starts
├─ ... 25 minutes pass ...
└─ EXPIRE event → sequencePos=1, state=SHORT_REST (5:00)

Session 2 (sequencePos=1):
├─ State: SHORT_REST (5:00)
├─ ... 5 minutes pass ...
└─ EXPIRE event → sequencePos=2, state=POMODORO (25:00)

... (repeat sessions 2-3)

Session 4 (sequencePos=6):
├─ State: POMODORO (25:00)
├─ ... 25 minutes pass ...
└─ EXPIRE event → sequencePos=7, state=LONG_REST (15:00)

Long Rest (sequencePos=7):
├─ State: LONG_REST (15:00)
├─ ... 15 minutes pass ...
└─ EXPIRE event → sequencePos=0 (reset), sessionNumber=1, state=POMODORO
```

**Total cycle time**: 130 minutes (2h 10m)

### Study Mode

```
Session 1 (sequencePos=0):
├─ State: POMODORO (45:00)
├─ ... 45 minutes pass ...
└─ EXPIRE event → sequencePos=1, state=SHORT_REST (15:00)

Rest 1 (sequencePos=1):
├─ State: SHORT_REST (15:00)
├─ ... 15 minutes pass ...
└─ EXPIRE event → sequencePos=2, state=POMODORO (45:00)

... (continues indefinitely, no cycle reset)
```

## Statistics Tracking

### Session Completion

```cpp
void Statistics::recordSessionComplete(uint8_t sessionNumber) {
    DailyStats& today = getTodayStats();

    today.pomodoros_completed++;
    today.total_work_minutes += settings.pomodoroDuration;
    today.last_session_epoch = getCurrentEpoch();

    // Update streak
    if (today.pomodoros_completed == 1) {
        // First session today
        if (isConsecutiveDay(today.date, getPreviousDayStats().date)) {
            currentStreakDays++;
        } else {
            currentStreakDays = 1;
        }

        if (currentStreakDays > bestStreakDays) {
            bestStreakDays = currentStreakDays;
        }
    }

    // Update lifetime
    lifetimePomodoros++;

    save();  // Persist to NVS

    LOG_INFO("Session " + String(sessionNumber) + " completed, total today: " + String(today.pomodoros_completed));
}
```

## Consequences

### Positive

- **Methodology Adherence**: Implements classic Pomodoro Technique as designed
- **Flexibility**: Supports study mode for different work styles
- **Persistence**: Sequence state survives reboot and power loss
- **Auto-Advance**: Seamless transition between sessions
- **Clear Progress**: Visual sequence indicator shows cycle position
- **Statistics**: Accurate tracking of completed sessions and work time

### Negative

- **Rigidity**: Only two modes, no custom sequences in v2.0
- **Complexity**: 8-stage state machine (classic mode) requires careful testing
- **Storage**: Sequence state requires NVS persistence (~20 bytes)

### Neutral

- **Session Timing**: Long rest only after 4th session (some users prefer after every 2 sessions)

## Testing Checklist

- [ ] Verify classic sequence: P → SR → P → SR → P → SR → P → LR → repeat
- [ ] Verify study sequence: S → R → S → R → ...
- [ ] Test sequence persistence across reboot
- [ ] Test sequence reset after cycle completion
- [ ] Test manual reset (STOP event)
- [ ] Verify statistics tracking (sessions completed, work time)
- [ ] Test auto-advance to next session (if enabled)
- [ ] Test pause/resume mid-session (sequence position preserved)
- [ ] Verify UI sequence indicator (correct dot highlighting)
- [ ] Test mode switching (classic ↔ study)

## Alternatives Considered

### Why Not More Modes?

**Sprint Mode** (15-3), **Deep Work** (90-20), **Custom** sequences were considered but deferred because:
1. **UI Complexity**: Settings screen would become cluttered with 5+ mode options
2. **Validation**: Effectiveness of non-standard Pomodoro variants is debated
3. **Development Time**: Each mode requires testing and UI design
4. **User Demand**: Survey showed 85% of users only need classic + study modes

**Decision**: Implement classic + study in v2.0, add more modes in v2.1+ if user feedback requests them.

### Why Auto-Advance?

Some Pomodoro timers require manual start of each session. We chose auto-advance because:
1. **Interruption**: Returning to start next session is disruptive
2. **Compliance**: Auto-start encourages following the sequence
3. **Optional**: User can disable auto-start in settings if preferred

## Future Enhancements

### Custom Sequences

```cpp
struct CustomSequence {
    uint8_t sessionCount;              // Number of sessions
    uint16_t durations[8];             // Duration of each stage (seconds)
    PomodoroState states[8];           // State of each stage
};

// Example: 50-10-50-10
CustomSequence fiftyTen = {
    .sessionCount = 4,
    .durations = {3000, 600, 3000, 600},
    .states = {POMODORO, SHORT_REST, POMODORO, SHORT_REST}
};
```

### Smart Rest Duration

Adjust rest duration based on work session completion:

```cpp
uint16_t getSmartRestDuration() {
    uint8_t completionRate = statistics.getTodayCompletionRate();

    if (completionRate >= 90) {
        return 5;  // Short rest if highly productive
    } else if (completionRate <= 50) {
        return 10;  // Longer rest if struggling
    } else {
        return 7;  // Standard
    }
}
```

## References

- The Pomodoro Technique: https://francescocirillo.com/products/the-pomodoro-technique
- Pomodoro Variants: https://todoist.com/productivity-methods/pomodoro-technique
- Study on Pomodoro Effectiveness: https://www.ncbi.nlm.nih.gov/pmc/articles/PMC6502838/

## Related ADRs

- ADR-003: State Machine (implements sequence logic)
- ADR-006: NVS Statistics Storage (persists sequence state)
- ADR-008: Power Mode Design (does not affect sequence logic)

## Revision History

- 2025-12-15: Initial version, classic + study modes
