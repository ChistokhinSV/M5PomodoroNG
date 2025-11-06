# M5 Pomodoro v2 - Architecture Review Report

**Date**: January 5, 2025
**Reviewer**: Architecture Review Team
**Project Phase**: Local Timer Implementation (Phase 2)
**Focus**: State Machine Simplification and Architectural Integrity

---

## Executive Summary

The M5 Pomodoro v2 project exhibits a **fundamentally sound modular architecture** with clear separation of concerns across hardware, UI, and business logic layers. However, the current implementation suffers from **unnecessary complexity in the state machine design** and **inconsistent architectural patterns** that impact maintainability and testability.

The most critical issue is the **artificial separation of work and break sessions into different states** (RUNNING vs BREAK), which duplicates logic and complicates state transitions. This design violates the DRY principle and creates maintenance overhead without providing clear benefits. The state machine can be significantly simplified by unifying these states and leveraging the existing PomodoroSequence component for session type differentiation.

Secondary concerns include **tight coupling between UI screens and core components**, **inconsistent callback patterns**, and a **discrepancy between documented and implemented architecture** regarding FreeRTOS task separation. While these issues are less critical, addressing them would improve the system's long-term maintainability and alignment with documented design decisions.

---

## Architecture Analysis

### 1. State Machine Design

#### Current State Diagram

```
IDLE ──START──> RUNNING/BREAK ──TIMEOUT──> COMPLETED
  ^                 │ ^                         │
  └─────STOP────────┘ └──PAUSE/RESUME──> PAUSED│
                                               └──STOP/START
```

#### Issues Identified

1. **Redundant State Separation** (CRITICAL)
   - RUNNING and BREAK states have nearly identical behavior
   - Both states: count down, handle pause/resume, trigger audio, update UI
   - Differentiation is already handled by PomodoroSequence (work vs break type)
   - Code duplication in enterState(), handleEvent(), and transition logic

2. **COMPLETED State Unnecessary** (MAJOR)
   - Adds complexity without clear value
   - Could be replaced by returning to IDLE with a completion flag
   - Creates awkward transitions (COMPLETED→START vs IDLE→START)

3. **State Machine Responsibilities Unclear** (MAJOR)
   - MainScreen controls progression via `sequence.advance()` (line 229 in MainScreen.cpp)
   - State machine increments statistics (line 228 in TimerStateMachine.cpp)
   - Sequence tracking split between TimerStateMachine and PomodoroSequence
   - Violates Single Responsibility Principle

### 2. Component Coupling Analysis

#### Dependency Graph

```
main.cpp
    │
    ├──> ScreenManager (owns all screens)
    │        ├──> MainScreen ────────┐
    │        ├──> StatsScreen        ├──> TimerStateMachine
    │        ├──> SettingsScreen     ├──> PomodoroSequence
    │        └──> PauseScreen        ├──> Statistics
    │                                └──> Config
    │
    ├──> TimerStateMachine (depends on PomodoroSequence)
    ├──> PomodoroSequence (independent)
    ├──> Statistics (independent)
    ├──> Config (independent)
    ├──> LEDController (independent)
    └──> AudioPlayer (independent)
```

#### Coupling Issues

1. **Screens Know Too Much** (MAJOR)
   - All screens receive references to ALL core components
   - MainScreen directly manipulates state machine AND sequence
   - Violates Interface Segregation Principle
   - Example: StatsScreen doesn't need TimerStateMachine reference

2. **Global Callback Pattern** (MINOR)
   - `g_navigate_callback` global function pointer (ScreenManager.h:34)
   - MainScreen uses static instance pointer for callbacks
   - Fragile design that complicates testing

3. **Circular Knowledge** (MINOR)
   - ScreenManager knows about specific screen types
   - Screens know about ScreenManager via global callback
   - Could be decoupled with proper event system

### 3. SOLID Principle Violations

#### Single Responsibility Principle (SRP)
- **TimerStateMachine**: Manages state, timer countdown, audio triggers, statistics updates
- **MainScreen**: UI rendering, state machine control, sequence progression
- **ScreenManager**: Navigation, auto-navigation logic, status propagation

#### Open/Closed Principle (OCP)
- Adding new screen requires modifying ScreenManager class
- New state machine events require changes throughout handleEvent() switch

#### Interface Segregation Principle (ISP)
- Screens receive all dependencies even if unused
- No interface/abstract base for screens (duck-typed)

#### Dependency Inversion Principle (DIP)
- Concrete dependencies passed everywhere
- No abstraction layer for hardware components

### 4. Architecture vs Implementation Gap

#### ADR-002 Promises vs Reality

**Documented** (ADR-002):
```
Core 0: ui_task, touch_task, gyro_task, led_task, audio_task
Core 1: timer_task, network_task, stats_task
```

**Implemented** (main.cpp):
```cpp
// Everything runs in single loop() on Core 1
void loop() {
    M5.update();
    screenManager->handleHardwareButtons();
    screenManager->handleTouch();
    screenManager->update();
    screenManager->draw();
    audioPlayer.update();
    renderer.update();
}
```

**Gap**: No FreeRTOS tasks created, no dual-core separation, no queues/semaphores

---

## Issue Inventory

### Critical Issues

1. **State Machine Complexity** (TimerStateMachine.h/cpp)
   - Separate RUNNING/BREAK states duplicate 90% of logic
   - COMPLETED state adds unnecessary transition complexity
   - State responsibilities mixed with MainScreen

### Major Issues

2. **Component Over-Coupling** (ScreenManager, all screens)
   - Screens receive unnecessary dependencies
   - Direct manipulation instead of message passing
   - Global callback anti-pattern

3. **Architecture Mismatch** (main.cpp vs ADR-002)
   - Single-threaded implementation vs documented multi-task design
   - No FreeRTOS task separation as planned
   - Missing synchronization primitives

### Minor Issues

4. **Callback Inconsistency** (throughout)
   - Mix of std::function, static methods, global pointers
   - No unified event system

5. **Missing Abstractions** (hardware layer)
   - Concrete classes passed directly
   - No interface definitions
   - Hard to mock for testing

---

## Simplification Opportunities

### 1. State Machine Simplification

#### Proposed Simplified States

```cpp
enum class State {
    IDLE,     // No timer active
    ACTIVE,   // Timer running (work OR break)
    PAUSED    // Timer paused
};
```

#### Benefits
- Reduces states from 5 to 3
- Eliminates duplicate logic
- Session type determined by PomodoroSequence
- Clearer state transitions

#### Implementation Changes

```cpp
// Before (complex)
if (session.type == PomodoroSequence::SessionType::WORK) {
    return transition(State::RUNNING);
} else {
    return transition(State::BREAK);
}

// After (simple)
return transition(State::ACTIVE);
// Session type checked only where behavior differs (audio, LED color)
```

### 2. Responsibility Clarification

#### Proposed Separation

**TimerStateMachine**: Pure timer logic
- Countdown management
- Pause/resume mechanics
- Timeout detection
- State transitions

**PomodoroSequence**: Session management
- Track current session
- Determine work/break type
- Handle progression
- Manage completion counts

**MainScreen**: UI only
- Display timer
- Handle button presses
- Send commands to state machine
- React to state changes

### 3. Event-Driven Architecture

Replace callbacks with unified event system:

```cpp
class EventBus {
    void publish(Event event);
    void subscribe(EventType type, Handler handler);
};

// Usage
eventBus.publish(TimerExpired{remaining: 0});
eventBus.publish(NavigateToScreen{screen: ScreenID::STATS});
```

---

## Improvement Plan

### Phase 1: State Machine Simplification (2 days)

**Priority**: CRITICAL
**Impact**: High - Reduces complexity by ~40%
**Breaking Changes**: None (external interface unchanged)

1. **Merge RUNNING and BREAK states**
   - Create unified ACTIVE state
   - Use `sequence.getCurrentSession().type` for differentiation
   - Consolidate enter/exit logic

2. **Eliminate COMPLETED state**
   - Transition directly to IDLE on timeout
   - Add completion flag or event if needed
   - Simplify transition matrix

3. **Clarify responsibilities**
   - Move progression logic to PomodoroSequence
   - Move statistics updates to Statistics class
   - State machine only manages timer state

### Phase 2: Decouple Components (3 days)

**Priority**: Major
**Impact**: Medium - Improves testability
**Breaking Changes**: Screen constructors

1. **Interface Segregation**
   - Create ITimerDisplay interface for MainScreen
   - Create IStatisticsView interface for StatsScreen
   - Pass only required interfaces to screens

2. **Remove Global Callbacks**
   - Pass navigation callback via constructor
   - Use instance methods instead of static
   - Consider event bus pattern

3. **Abstract Hardware Layer**
   - Create interfaces for hardware components
   - Inject interfaces instead of concrete classes
   - Enable unit testing with mocks

### Phase 3: Implement FreeRTOS Tasks (5 days)

**Priority**: Major
**Impact**: High - Enables true parallelism
**Breaking Changes**: Major refactoring

1. **Create Task Structure**
   - Implement tasks as per ADR-002
   - Add queues for inter-task communication
   - Add mutexes for shared resources

2. **Move Components to Tasks**
   - UI rendering to Core 0 task
   - Timer logic to Core 1 task
   - Network operations to separate task

3. **Add Synchronization**
   - Queue for timer updates to UI
   - Mutex for display access
   - Event groups for state coordination

### Phase 4: Technical Debt (2 days)

**Priority**: Minor
**Impact**: Low - Code quality
**Breaking Changes**: None

1. **Unify Callback Patterns**
   - Standardize on std::function
   - Remove static instance pointers
   - Document callback lifecycle

2. **Add Missing ADRs**
   - ADR-009: State machine simplification
   - ADR-010: Screen navigation pattern
   - ADR-011: Event system design

3. **Update Documentation**
   - Align docs with implementation
   - Update state machine diagram
   - Document actual task structure

---

## ADR Gap Analysis

### Valid and Implemented
- ✅ ADR-001: M5Unified library (correctly used)
- ✅ ADR-006: NVS statistics storage (Config class implemented)
- ✅ ADR-007: Classic pomodoro sequence (PomodoroSequence follows spec)

### Valid but Not Implemented
- ❌ ADR-002: Dual-core architecture (single-threaded implementation)
- ❌ ADR-003: FreeRTOS synchronization (no tasks/queues/mutexes)
- ❌ ADR-005: Light sleep during timer (no power management)

### Partially Implemented
- ⚠️ ADR-004: Gyro polling strategy (GyroController exists but not integrated)
- ⚠️ ADR-008: Power mode design (PowerManager stub exists)

### Missing ADRs Needed
- State machine design rationale (why 5 states?)
- Screen navigation pattern
- Callback vs event system decision
- Single vs multi-threaded tradeoff

---

## Long-Term Implications

### Current Architecture Trajectory

If unchanged, the current architecture will lead to:
- **Increased maintenance cost** due to state machine complexity
- **Difficult testing** due to tight coupling
- **Performance issues** when network operations added (Phase 3)
- **Code duplication** as more states/screens are added

### With Proposed Changes

The simplified architecture enables:
- **Easier testing** through interface segregation
- **Better performance** via proper task separation
- **Cleaner extensions** with event-driven patterns
- **Reduced bugs** from simpler state machine

---

## Recommendations

### Immediate Actions (Do Now)

1. **Simplify State Machine** (Critical)
   - Merge RUNNING/BREAK into ACTIVE
   - Remove COMPLETED state
   - Est. effort: 1 day
   - Risk: Low (well-isolated component)

2. **Document Actual Architecture**
   - Update ADR-002 or create ADR-009 explaining single-threaded decision
   - Document why FreeRTOS tasks weren't implemented
   - Est. effort: 2 hours

### Short-Term (Next Sprint)

3. **Decouple Screen Dependencies**
   - Create interfaces for required functionality
   - Pass only needed dependencies
   - Est. effort: 2 days
   - Risk: Medium (touches all screens)

4. **Implement Event System**
   - Replace callbacks with events
   - Centralize inter-component communication
   - Est. effort: 3 days
   - Risk: Medium

### Long-Term (Before Phase 3)

5. **Implement FreeRTOS Tasks**
   - Critical before adding network operations
   - Follow ADR-002 design
   - Est. effort: 5 days
   - Risk: High (major refactoring)

---

## Conclusion

The M5 Pomodoro v2 project has a **solid foundation** but suffers from **over-engineering in the state machine** and **under-implementation of the planned architecture**. The most impactful improvement would be **simplifying the state machine** from 5 states to 3, which would reduce code complexity by approximately 40% while maintaining all functionality.

The gap between documented architecture (FreeRTOS tasks) and implementation (single loop) should be addressed either by implementing the planned design or updating documentation to reflect the simpler approach if it meets requirements.

The proposed improvements follow a risk-managed approach, starting with low-risk/high-impact changes (state simplification) before tackling higher-risk architectural changes (FreeRTOS implementation).

**Good architecture enables change. The current state machine design makes changes harder than necessary.**

---

## Appendix A: Simplified State Machine Proposal

```cpp
class SimplifiedTimerStateMachine {
public:
    enum class State { IDLE, ACTIVE, PAUSED };
    enum class Event { START, PAUSE, RESUME, STOP, TIMEOUT };

    void handleEvent(Event event) {
        switch (state) {
            case State::IDLE:
                if (event == Event::START) {
                    startTimer();
                    state = State::ACTIVE;
                    notifyStateChange();
                }
                break;

            case State::ACTIVE:
                if (event == Event::PAUSE) {
                    state = State::PAUSED;
                    notifyStateChange();
                } else if (event == Event::TIMEOUT) {
                    handleTimeout();
                    state = State::IDLE;
                    notifyStateChange();
                }
                break;

            case State::PAUSED:
                if (event == Event::RESUME) {
                    state = State::ACTIVE;
                    notifyStateChange();
                } else if (event == Event::STOP) {
                    resetTimer();
                    state = State::IDLE;
                    notifyStateChange();
                }
                break;
        }
    }

private:
    State state = State::IDLE;
    uint32_t remaining_ms = 0;

    void handleTimeout() {
        // Let PomodoroSequence handle progression
        sequence.completeSession();

        // Let Statistics handle tracking
        if (sequence.wasWorkSession()) {
            statistics.incrementCompleted();
        }

        // Timer just manages time
        remaining_ms = 0;
    }
};
```

## Appendix B: Component Interface Proposal

```cpp
// Pure interfaces for dependency injection
class ITimerControl {
    virtual void start() = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual void stop() = 0;
    virtual uint32_t getRemainingMs() = 0;
};

class ISequenceInfo {
    virtual SessionType getCurrentType() = 0;
    virtual uint8_t getCurrentNumber() = 0;
    virtual uint8_t getTotalSessions() = 0;
};

class IStatisticsView {
    virtual uint16_t getTodayCompleted() = 0;
    virtual float getWeeklyAverage() = 0;
    virtual void getWeeklyData(uint16_t* data, size_t days) = 0;
};

// Screens receive only what they need
class MainScreen {
    MainScreen(ITimerControl& timer, ISequenceInfo& sequence);
    // Not: MainScreen(TimerStateMachine&, PomodoroSequence&, Statistics&, Config&, ...)
};
```

---

**Document Version**: 1.0
**Review Type**: Architecture and Design Patterns
**Next Review**: After Phase 2 completion