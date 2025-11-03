# State Machine Design - M5 Pomodoro v2

## Document Information
- **Version**: 1.0
- **Date**: 2025-01-03
- **Status**: Draft
- **Related**: [02-ARCHITECTURE.md](02-ARCHITECTURE.md), [01-TECHNICAL-REQUIREMENTS.md](01-TECHNICAL-REQUIREMENTS.md)

---

## 1. Overview

The M5 Pomodoro v2 timer state machine manages the core business logic of the Pomodoro technique, including work sessions, rest periods, and the classic 4-session sequence. This document defines all states, events, transitions, and the sequence tracking logic.

---

## 2. State Definitions

### 2.1 Timer States

```cpp
enum class PomodoroState {
    STOPPED,      // Idle, ready to start new session
    POMODORO,     // Active work session (25/45 minutes)
    SHORT_REST,   // Short break (5 minutes, after sessions 1-3)
    LONG_REST,    // Long break (15 minutes, after session 4)
    PAUSED        // Timer paused, time frozen
};
```

#### STOPPED
**Description**: Initial state, timer idle, waiting for user to start
**Characteristics**:
- No active timer
- Sequence position preserved (e.g., ready for session 2)
- Screen shows "Ready" or configured duration
- LED strip off or ambient mode
- Low power consumption

**Entry Actions**:
- Stop timer ticker
- Clear remaining time
- Update UI to show "Start" button
- LED animation: fade to off

**Exit Actions**:
- Record session start timestamp
- Start timer ticker (1-second interval)

#### POMODORO
**Description**: Active work session in progress
**Characteristics**:
- Timer counting down from configured duration
- Full screen timer display with progress bar
- LED strip: green breathing animation
- Gyro gestures active (rotation to pause)
- Statistics tracking enabled (will increment on completion)

**Entry Actions**:
- Load work duration from config (25/45 min based on mode)
- Set start timestamp to current epoch
- Calculate expiration time
- Play start.wav audio
- Update UI: large timer display
- LED: Start green breathe animation
- Publish state to cloud (if connected)

**During**:
- Every second: decrement remaining time
- Every 30 seconds: Update progress bar
- At 30 seconds remaining: Play warning.wav audio
- Monitor gyro for pause gesture

**Exit Actions**:
- Stop timer ticker (if paused/stopped)
- On completion: Increment statistics
- On completion: Advance sequence session

#### SHORT_REST
**Description**: 5-minute break after work sessions 1-3
**Characteristics**:
- Timer counting down from 5 minutes
- Screen shows rest message + timer
- LED strip: blue slow fade animation
- Optional auto-start next session (configurable)

**Entry Actions**:
- Set duration to short_rest_minutes (default 5)
- Set start timestamp
- Play rest.wav audio
- Update UI: "Take a break!" message
- LED: Start blue fade animation
- Publish state to cloud

**During**:
- Every second: decrement remaining time
- At 10 seconds remaining: Show "Get ready" message
- Monitor gyro for flat detection (start next session)

**Exit Actions**:
- On completion: Check auto_start_next config
  - If true: Transition to POMODORO (next session)
  - If false: Transition to STOPPED (wait for user)

#### LONG_REST
**Description**: 15-minute break after 4th work session (sequence complete)
**Characteristics**:
- Timer counting down from 15 minutes
- Screen shows "Great job! 4 sessions complete"
- LED strip: rainbow celebration animation (first 5 seconds), then blue fade
- Sequence resets to session 1 after completion

**Entry Actions**:
- Set duration to long_rest_minutes (default 15)
- Set start timestamp
- Play long_rest.wav audio
- Update UI: Celebration message + confetti animation
- LED: Rainbow wipe, then blue fade
- Publish state to cloud
- Log sequence completion milestone

**During**:
- Every second: decrement remaining time
- Display motivational quote (optional)
- Monitor gyro for flat detection (start new sequence)

**Exit Actions**:
- Reset sequence to session 1
- Transition to STOPPED

#### PAUSED
**Description**: Timer paused, time frozen
**Characteristics**:
- Timer display shows remaining time (frozen)
- Screen has amber tint/overlay
- LED strip: amber pulsing
- Preserves exact remaining seconds
- Can resume to exact point

**Entry Actions**:
- Stop timer ticker
- Save remaining_seconds to state
- Update UI: Show "PAUSED" label, amber overlay
- LED: Start amber pulse animation
- Disable auto-sleep (prevent screen turning off)
- Publish paused state to cloud

**During**:
- Display remains on (no sleep)
- Monitor gyro for flat detection (resume)
- Monitor touch for resume button

**Exit Actions**:
- On RESUME: Recalculate expiration time from remaining_seconds
- On STOP: Discard remaining time, reset

---

## 3. Event Definitions

### 3.1 Timer Events

```cpp
enum class TimerEvent {
    START,        // User initiates new session
    PAUSE,        // User pauses running timer
    RESUME,       // User resumes from pause
    STOP,         // User stops timer, reset to STOPPED
    EXPIRE,       // Timer reaches 00:00
    GYRO_ROTATE,  // Gyro rotation detected (context-dependent)
    GYRO_FLAT,    // Device laid flat (context-dependent)
    GYRO_SHAKE    // Double-shake detected (emergency stop)
};
```

#### START Event
**Trigger**:
- Touch "Start" button on main screen
- Gyro rotation gesture (from STOPPED)
- HMI button A press
- Cloud command (phone app)

**Preconditions**:
- Current state is STOPPED
- Config loaded (durations set)
- Time manager initialized (has valid epoch)

**Data**:
```cpp
struct StartEventData {
    PomodoroMode mode;    // CLASSIC, STUDY, or CUSTOM
    uint16_t duration_s;  // Optional override (0 = use config)
    char task_name[64];   // Optional task description
};
```

#### PAUSE Event
**Trigger**:
- Touch "Pause" button
- Gyro rotation gesture (from POMODORO/REST)
- HMI button B press
- Cloud command

**Preconditions**:
- Current state is POMODORO or SHORT_REST or LONG_REST
- Not already paused

**Data**: None

#### RESUME Event
**Trigger**:
- Touch "Resume" button
- Gyro flat detection (lay device flat)
- HMI button A press
- Cloud command

**Preconditions**:
- Current state is PAUSED
- Remaining time > 0

**Data**: None

#### STOP Event
**Trigger**:
- Touch "Stop" button
- Gyro double-shake (emergency stop)
- HMI button B long-press (2 seconds)
- Cloud command

**Preconditions**:
- Current state is POMODORO, REST, or PAUSED
- Not already STOPPED

**Data**:
```cpp
struct StopEventData {
    bool is_interruption;  // true = stopped mid-session (for statistics)
};
```

#### EXPIRE Event
**Trigger**:
- Timer countdown reaches 00:00 (generated by TimerStateMachine.tick())

**Preconditions**:
- Current state is POMODORO, SHORT_REST, or LONG_REST
- Remaining time == 0

**Data**: None (state machine knows current state)

#### GYRO_ROTATE Event
**Trigger**:
- GyroController detects ±60° rotation from vertical

**Behavior** (context-dependent):
- From STOPPED: Trigger START
- From POMODORO/REST: Trigger PAUSE
- From PAUSED: (ignored, use GYRO_FLAT for resume)

**Data**:
```cpp
struct GyroRotateData {
    float angle_degrees;  // Actual rotation angle
    GyroDirection direction; // LEFT or RIGHT
};
```

#### GYRO_FLAT Event
**Trigger**:
- GyroController detects horizontal position (±10°) held for 1 second

**Behavior** (context-dependent):
- From SHORT_REST: Start next session (if at end of rest)
- From LONG_REST: Start new sequence (session 1)
- From PAUSED: Trigger RESUME
- From STOPPED: (ignored)

**Data**: None

#### GYRO_SHAKE Event
**Trigger**:
- GyroController detects double-shake gesture (two rapid shakes <1 second apart)

**Behavior**:
- From ANY state: Emergency STOP
- Provides strong haptic + visual feedback

**Data**: None

---

## 4. State Transition Table

### 4.1 Complete Transition Matrix

| Current State | Event | Next State | Condition | Action |
|--------------|-------|------------|-----------|--------|
| **STOPPED** | START | POMODORO | Config valid | Start work timer |
| STOPPED | PAUSE | STOPPED | - | Ignored |
| STOPPED | RESUME | STOPPED | - | Ignored |
| STOPPED | STOP | STOPPED | - | Ignored (already stopped) |
| STOPPED | EXPIRE | STOPPED | - | Ignored |
| STOPPED | GYRO_ROTATE | POMODORO | - | Same as START |
| STOPPED | GYRO_FLAT | STOPPED | - | Ignored |
| STOPPED | GYRO_SHAKE | STOPPED | - | Ignored (already stopped) |
| **POMODORO** | START | POMODORO | - | Ignored (already running) |
| POMODORO | PAUSE | PAUSED | - | Freeze timer, save remaining |
| POMODORO | RESUME | POMODORO | - | Ignored (not paused) |
| POMODORO | STOP | STOPPED | - | Interrupt, mark incomplete |
| POMODORO | EXPIRE | SHORT_REST or LONG_REST | Session 1-3: SHORT_REST, Session 4: LONG_REST | Increment stats, advance session |
| POMODORO | GYRO_ROTATE | PAUSED | - | Same as PAUSE |
| POMODORO | GYRO_FLAT | POMODORO | - | Ignored |
| POMODORO | GYRO_SHAKE | STOPPED | - | Emergency stop |
| **SHORT_REST** | START | SHORT_REST | - | Ignored (rest in progress) |
| SHORT_REST | PAUSE | PAUSED | - | Freeze rest timer |
| SHORT_REST | RESUME | SHORT_REST | - | Ignored |
| SHORT_REST | STOP | STOPPED | - | Stop rest early |
| SHORT_REST | EXPIRE | STOPPED or POMODORO | auto_start_next: false → STOPPED, true → POMODORO | Prepare for next session |
| SHORT_REST | GYRO_ROTATE | PAUSED | - | Same as PAUSE |
| SHORT_REST | GYRO_FLAT | POMODORO | Rest complete | Start next session |
| SHORT_REST | GYRO_SHAKE | STOPPED | - | Emergency stop |
| **LONG_REST** | START | LONG_REST | - | Ignored |
| LONG_REST | PAUSE | PAUSED | - | Freeze rest timer |
| LONG_REST | RESUME | LONG_REST | - | Ignored |
| LONG_REST | STOP | STOPPED | - | Stop rest early |
| LONG_REST | EXPIRE | STOPPED | - | Reset sequence to session 1 |
| LONG_REST | GYRO_ROTATE | PAUSED | - | Same as PAUSE |
| LONG_REST | GYRO_FLAT | POMODORO | Rest complete | Start new sequence (session 1) |
| LONG_REST | GYRO_SHAKE | STOPPED | - | Emergency stop |
| **PAUSED** | START | PAUSED | - | Ignored |
| PAUSED | PAUSE | PAUSED | - | Ignored (already paused) |
| PAUSED | RESUME | POMODORO | Was in POMODORO | Resume with remaining time |
| PAUSED | RESUME | SHORT_REST | Was in SHORT_REST | Resume rest |
| PAUSED | RESUME | LONG_REST | Was in LONG_REST | Resume rest |
| PAUSED | STOP | STOPPED | - | Discard remaining time |
| PAUSED | EXPIRE | PAUSED | - | Ignored (timer frozen) |
| PAUSED | GYRO_ROTATE | PAUSED | - | Ignored (already paused) |
| PAUSED | GYRO_FLAT | (previous state) | - | Same as RESUME |
| PAUSED | GYRO_SHAKE | STOPPED | - | Emergency stop |

### 4.2 Transition Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                        STOPPED                               │
│         (Idle, ready for next session)                       │
└──────────────┬──────────────────────────────────────────────┘
               │
               │ START / GYRO_ROTATE
               │
               ▼
┌─────────────────────────────────────────────────────────────┐
│                       POMODORO                               │
│              (Work session: 25/45 min)                       │
└──────────────┬──────────────────────────────────────────────┘
               │
               ├─ PAUSE / GYRO_ROTATE ──→ PAUSED ──┐
               │                            │       │
               │ EXPIRE                     │ RESUME│
               ▼                            ▼       │
┌────────────────────────┐         ┌──────────────┴─┐
│   SHORT_REST (5 min)   │◄────────┤                │
│  (after sessions 1-3)  │         │                │
└────────────────────────┘         │                │
               │                   │                │
               │ EXPIRE / GYRO_FLAT │               │
               ▼                   │                │
┌─────────────────────────────────┐│                │
│         POMODORO                ││                │
│      (next session)             ││                │
└─────────────────────────────────┘│                │
               │                   │                │
               │ (After session 4) │                │
               ▼                   │                │
┌────────────────────────┐         │                │
│   LONG_REST (15 min)   │◄────────┘                │
│  (sequence complete)   │                          │
└────────────────────────┘                          │
               │                                    │
               │ EXPIRE                             │
               ▼                                    │
         [STOPPED]                                  │
         (Reset to session 1)                      │
                                                    │
               STOP / GYRO_SHAKE (from any state)  │
               ─────────────────────────────────────┘
                              │
                              ▼
                          STOPPED
```

---

## 5. Sequence Tracking

### 5.1 Pomodoro Sequence Structure

```cpp
struct PomodoroSequence {
    PomodoroMode mode;           // CLASSIC, STUDY, or CUSTOM
    uint8_t current_session;     // 1-4 for CLASSIC, 0 for STUDY
    uint8_t completed_today;     // Total completed pomodoros today
    uint32_t sequence_start;     // Epoch timestamp when sequence started
    uint8_t interruptions_today; // Stopped mid-session count
};
```

### 5.2 Classic Mode Sequence Logic

**4-Session Cycle**:
```
Session 1: 25 min POMODORO → 5 min SHORT_REST
Session 2: 25 min POMODORO → 5 min SHORT_REST
Session 3: 25 min POMODORO → 5 min SHORT_REST
Session 4: 25 min POMODORO → 15 min LONG_REST
[Reset to Session 1]
```

**Implementation**:
```cpp
void advanceSession() {
    if (mode == CLASSIC) {
        if (current_session < 4) {
            current_session++;
            // Transition to SHORT_REST
        } else {
            current_session = 1;  // Reset sequence
            // Transition to LONG_REST
        }
    } else if (mode == STUDY) {
        // Study mode has no sessions, just repeating cycle
        // Always transition to STUDY_REST (15 min)
    }
}
```

### 5.3 Study Mode Sequence Logic

**Repeating Cycle** (no session tracking):
```
45 min POMODORO → 15 min REST → 45 min POMODORO → 15 min REST → ...
```

**Implementation**:
```cpp
// Study mode: current_session always 0
void advanceStudyMode() {
    // Simply toggle between POMODORO and REST
    // No sequence completion concept
}
```

### 5.4 Sequence Events

**Sequence Started**:
- User starts first pomodoro of the day
- Action: Set sequence_start = current_epoch

**Sequence Advanced**:
- Pomodoro completes (EXPIRE event)
- Action: Increment current_session (if CLASSIC)

**Sequence Completed**:
- Session 4 pomodoro completes
- Action: Trigger LONG_REST, reset current_session = 1

**Sequence Reset**:
- User manually resets (settings menu)
- Midnight rollover (00:00 UTC)
- Action: Set current_session = 1, clear completed_today

**Sequence Interrupted**:
- User stops mid-pomodoro (STOP event, is_interruption=true)
- Action: Increment interruptions_today, do NOT increment completed_today

---

## 6. Timer Mechanics

### 6.1 Time Representation

**Epoch-Based Timing**:
```cpp
struct TimerState {
    uint32_t start_epoch;      // UTC epoch when timer started
    uint16_t duration_seconds; // Total duration (e.g., 1500 for 25 min)
    uint16_t remaining_seconds; // Calculated or saved (if paused)
};
```

**Calculation**:
```cpp
uint32_t elapsed = current_epoch - start_epoch;
uint16_t remaining = duration_seconds - elapsed;

if (remaining <= 0) {
    // Timer expired
    handleEvent(EXPIRE);
}
```

**Advantages**:
- Survives reboot (if start_epoch saved to RTC memory)
- Immune to clock adjustments (NTP sync recalculates)
- No accumulation of tick errors

### 6.2 NTP Drift Correction

**Problem**: NTP sync adjusts clock, throwing off timer

**Solution**: Shift timer start timestamp by correction delta
```cpp
void onNTPSync(int32_t correction_seconds) {
    if (state == POMODORO || state == SHORT_REST || state == LONG_REST) {
        // Adjust start timestamp to compensate
        start_epoch += correction_seconds;
        // Timer continues as if nothing happened
    }
}
```

### 6.3 Pause Mechanics

**On Pause**:
```cpp
void onPause() {
    // Calculate and save remaining time
    uint32_t elapsed = rtc.getEpoch() - start_epoch;
    remaining_seconds = duration_seconds - elapsed;

    // Stop ticker
    timerTicker.detach();

    // Transition to PAUSED state
    state = PAUSED;
    paused_from_state = previous_state; // Save for resume
}
```

**On Resume**:
```cpp
void onResume() {
    // Recalculate start time based on remaining seconds
    start_epoch = rtc.getEpoch() - (duration_seconds - remaining_seconds);

    // Restart ticker
    timerTicker.attach(1.0, []{ stateMachine.tick(); });

    // Transition back to previous state
    state = paused_from_state;
}
```

---

## 7. State Machine Implementation

### 7.1 Class Structure

```cpp
class TimerStateMachine {
public:
    // Constructor
    TimerStateMachine(PomodoroSequence* seq, Config* cfg);

    // Event handlers
    void handleEvent(TimerEvent event, void* data = nullptr);
    void tick();  // Called every second by ticker

    // State queries
    PomodoroState getState() const;
    uint16_t getRemainingSeconds() const;
    uint8_t getProgressPercent() const;

    // Callbacks (set by UI or other components)
    void onStateChanged(std::function<void(PomodoroState)> callback);
    void onTimerTick(std::function<void(uint16_t)> callback);

private:
    PomodoroState state;
    PomodoroState paused_from_state;
    uint32_t start_epoch;
    uint16_t duration_seconds;
    uint16_t remaining_seconds; // Only used when paused

    PomodoroSequence* sequence;
    Config* config;

    // Internal transition methods
    void transitionTo(PomodoroState new_state);
    void startTimer(uint16_t duration_s);
    void stopTimer();
    void playAudio(const char* filename);
    void publishState(); // Notify cloud via queue
};
```

### 7.2 Event Handling

```cpp
void TimerStateMachine::handleEvent(TimerEvent event, void* data) {
    switch (state) {
        case STOPPED:
            if (event == START || event == GYRO_ROTATE) {
                // Load config, start pomodoro
                uint16_t duration = (sequence->mode == CLASSIC) ?
                    config->classic_work_min * 60 : config->study_work_min * 60;
                startTimer(duration);
                transitionTo(POMODORO);
            }
            break;

        case POMODORO:
            if (event == PAUSE || event == GYRO_ROTATE) {
                uint32_t elapsed = rtc.getEpoch() - start_epoch;
                remaining_seconds = duration_seconds - elapsed;
                stopTimer();
                paused_from_state = POMODORO;
                transitionTo(PAUSED);
            } else if (event == STOP || event == GYRO_SHAKE) {
                stopTimer();
                sequence->interruptions_today++;
                transitionTo(STOPPED);
            } else if (event == EXPIRE) {
                stopTimer();
                sequence->completed_today++;
                sequence->advanceSession();

                // Determine next state based on session
                if (sequence->mode == CLASSIC && sequence->current_session == 1) {
                    // Just completed session 4, long rest
                    startTimer(config->long_rest_min * 60);
                    transitionTo(LONG_REST);
                } else {
                    // Completed session 1-3, short rest
                    startTimer(config->short_rest_min * 60);
                    transitionTo(SHORT_REST);
                }
            }
            break;

        case PAUSED:
            if (event == RESUME || event == GYRO_FLAT) {
                start_epoch = rtc.getEpoch() - (duration_seconds - remaining_seconds);
                startTimer(duration_seconds); // Re-attach ticker
                transitionTo(paused_from_state);
            } else if (event == STOP || event == GYRO_SHAKE) {
                stopTimer();
                transitionTo(STOPPED);
            }
            break;

        // ... handle SHORT_REST, LONG_REST similarly
    }
}
```

### 7.3 Ticker Implementation

```cpp
void TimerStateMachine::tick() {
    if (state == PAUSED || state == STOPPED) {
        return; // Don't update time when not running
    }

    uint32_t elapsed = rtc.getEpoch() - start_epoch;
    uint16_t remaining = duration_seconds - elapsed;

    if (remaining <= 0) {
        handleEvent(EXPIRE);
    } else {
        // Update UI via callback
        if (onTimerTickCallback) {
            onTimerTickCallback(remaining);
        }

        // Play warning at 30 seconds
        if (remaining == 30) {
            playAudio("/warning.wav");
        }
    }
}
```

---

## 8. Edge Cases and Error Handling

### 8.1 Edge Case: Clock Adjustment During Timer

**Scenario**: User adjusts system clock or NTP sync corrects time while timer running

**Solution**:
```cpp
void TimeManager::onNTPSync(int32_t correction_s) {
    if (stateMachine.isRunning()) {
        stateMachine.shiftStartTime(correction_s);
    }
}
```

### 8.2 Edge Case: Midnight Rollover

**Scenario**: Timer spans midnight (e.g., start 23:50, end 00:15)

**Solution**: Use epoch timestamps (no date boundary issues)
```cpp
// Epoch-based calculation handles midnight naturally
uint32_t start_epoch = 1735940400; // 2025-01-03 23:50:00
uint32_t current_epoch = 1735941900; // 2025-01-04 00:15:00
uint32_t elapsed = current_epoch - start_epoch; // 1500 seconds (25 min)
```

**Sequence Reset**: Separate logic checks date change
```cpp
void onDayChange() {
    if (statistics.getDate() != rtc.getDate()) {
        sequence.reset(); // Start fresh day
        statistics.createNewDay();
    }
}
```

### 8.3 Edge Case: Simultaneous Events

**Scenario**: User presses pause while timer expires (PAUSE + EXPIRE simultaneously)

**Solution**: Event queue with priority
```cpp
// Priority order: EXPIRE > STOP > PAUSE > RESUME > START
void handleEvent(TimerEvent event) {
    if (event == EXPIRE) {
        // Clear any pending pause/resume events
        eventQueue.clear();
    }
    // Process highest priority event
}
```

### 8.4 Edge Case: Deep Sleep During Timer

**Scenario**: Device enters deep sleep while timer running (inactivity timeout)

**Solution**: Save state to RTC memory before sleep
```cpp
void onDeepSleep() {
    if (state == POMODORO || state == SHORT_REST || state == LONG_REST) {
        // Save to RTC memory (survives deep sleep)
        rtcMemory.state = state;
        rtcMemory.start_epoch = start_epoch;
        rtcMemory.duration_seconds = duration_seconds;
        rtcMemory.was_running = true;
    }
}

void onWake() {
    if (rtcMemory.was_running) {
        // Restore timer state
        state = rtcMemory.state;
        start_epoch = rtcMemory.start_epoch;
        duration_seconds = rtcMemory.duration_seconds;

        // Check if expired during sleep
        uint32_t elapsed = rtc.getEpoch() - start_epoch;
        if (elapsed >= duration_seconds) {
            handleEvent(EXPIRE);
        } else {
            // Continue where we left off
            startTimer(duration_seconds);
        }
    }
}
```

### 8.5 Edge Case: Invalid State Recovery

**Scenario**: Corruption or unexpected state (e.g., NVS data corrupted)

**Solution**: Watchdog and safe defaults
```cpp
void recoverFromInvalidState() {
    logger.error("Invalid state detected, recovering...");

    // Reset to known good state
    state = STOPPED;
    start_epoch = 0;
    duration_seconds = 0;
    remaining_seconds = 0;

    // Reset sequence to session 1
    sequence.reset();

    // Notify user
    ui.showErrorToast("Timer reset due to error");
}
```

---

## 9. Testing Scenarios

### 9.1 State Transition Tests

| Test | Initial State | Event | Expected State | Verification |
|------|--------------|-------|----------------|--------------|
| T1 | STOPPED | START | POMODORO | Timer starts, LED green |
| T2 | POMODORO | PAUSE | PAUSED | Time frozen, LED amber |
| T3 | PAUSED | RESUME | POMODORO | Time continues from pause |
| T4 | POMODORO | EXPIRE (session 1) | SHORT_REST | Session advances to 2 |
| T5 | SHORT_REST | EXPIRE | STOPPED | Session 2 ready |
| T6 | POMODORO | EXPIRE (session 4) | LONG_REST | Sequence completes |
| T7 | LONG_REST | EXPIRE | STOPPED | Sequence resets to 1 |
| T8 | POMODORO | STOP | STOPPED | Interruption counted |
| T9 | PAUSED | STOP | STOPPED | Remaining time discarded |
| T10 | ANY | GYRO_SHAKE | STOPPED | Emergency stop works |

### 9.2 Sequence Logic Tests

| Test | Scenario | Expected Behavior |
|------|----------|-------------------|
| S1 | Complete sessions 1-3 | Each followed by 5-min SHORT_REST |
| S2 | Complete session 4 | Followed by 15-min LONG_REST |
| S3 | Complete LONG_REST | Sequence resets to session 1 |
| S4 | Pause during session 2, resume | Session remains at 2 |
| S5 | Stop during session 3 | Interruption counted, session stays 3 |
| S6 | Study mode pomodoro | No session tracking (always 0) |
| S7 | Midnight during session 2 | Sequence resets to 1 next START |

### 9.3 Edge Case Tests

| Test | Scenario | Expected Behavior |
|------|----------|-------------------|
| E1 | NTP sync +30s during POMODORO | Timer continues, no jump |
| E2 | Deep sleep 10 min into 25 min POMODORO | Wake, resume at 15 min remaining |
| E3 | Deep sleep 30 min into 25 min POMODORO | Wake, auto-expire, advance to REST |
| E4 | Simultaneous PAUSE + EXPIRE | EXPIRE wins, advances sequence |
| E5 | Battery dies during POMODORO | On reboot, recover or reset gracefully |

---

## 10. Performance Requirements

### 10.1 Timing Accuracy

- **Tick precision**: ±1 second per tick (acceptable for Pomodoro)
- **Total drift**: <30 seconds over 25-minute session (with NTP sync)
- **Pause/resume error**: 0 seconds (exact preservation)

### 10.2 Response Times

- **Event handling**: <50ms from event to state transition
- **UI update**: <100ms from state change to screen update
- **Cloud publish**: <500ms from state change to MQTT publish (when connected)

### 10.3 Resource Usage

- **State machine RAM**: <2KB (state struct + sequence struct)
- **Event queue**: 10 events max (FIFO, each ~20 bytes = 200 bytes)
- **Ticker overhead**: <5% CPU time (1-second interval)

---

## 11. Integration Points

### 11.1 UI Integration

```cpp
// UI subscribes to state changes
stateMachine.onStateChanged([](PomodoroState newState) {
    screenManager.updateForState(newState);
});

// UI subscribes to timer ticks
stateMachine.onTimerTick([](uint16_t remaining_s) {
    mainScreen.updateTimer(remaining_s);
});
```

### 11.2 Statistics Integration

```cpp
// Statistics listens for completion events
stateMachine.onStateChanged([](PomodoroState newState) {
    if (newState == SHORT_REST || newState == LONG_REST) {
        // Previous state was POMODORO and it expired
        statistics.incrementCompleted();
    }
});
```

### 11.3 Cloud Integration

```cpp
// Network task subscribes to state changes
stateMachine.onStateChanged([](PomodoroState newState) {
    ShadowUpdate update = {newState, rtc.getEpoch(), duration};
    xQueueSend(shadowPublishQueue, &update, 0);
});
```

### 11.4 Hardware Integration

```cpp
// LED controller reacts to state changes
stateMachine.onStateChanged([](PomodoroState newState) {
    switch(newState) {
        case POMODORO: ledController.startBreatheAnimation(GREEN); break;
        case SHORT_REST:
        case LONG_REST: ledController.startFadeAnimation(BLUE); break;
        case PAUSED: ledController.startPulseAnimation(AMBER); break;
        case STOPPED: ledController.off(); break;
    }
});

// Audio player handles state transitions
stateMachine.onStateChanged([](PomodoroState newState) {
    if (newState == POMODORO) audioPlayer.play("/start.wav");
    else if (newState == SHORT_REST) audioPlayer.play("/rest.wav");
    else if (newState == LONG_REST) audioPlayer.play("/long_rest.wav");
});
```

---

## 12. Future Enhancements

### 12.1 Extended Pomodoro Modes

**Pomodoro + Plus**:
- Allow 5th, 6th... sessions before long rest
- Configurable sessions per cycle (2-8)

**Adaptive Mode**:
- Shorten/lengthen work sessions based on completion rate
- AI-suggested break timing

### 12.2 State Persistence

**Cloud State Sync**:
- Resume timer from phone if device off
- Multi-device sequence synchronization

**Session History**:
- Track all session starts/stops
- Export to CSV for analysis

### 12.3 Advanced Transitions

**Conditional Transitions**:
- Skip rest if user chooses
- Extend pomodoro by 5 minutes (one-time)

**Scheduled Transitions**:
- Auto-start at specific times (e.g., 9:00 AM)
- Lunch break auto-scheduling

---

**Document Version**: 1.0
**Last Updated**: 2025-01-03
**Status**: APPROVED
**Implementation**: MP-10 (Sequence), MP-11 (State Machine)
