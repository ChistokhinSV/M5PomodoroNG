# ADR-009: Settings Management Single Responsibility Architecture

**Status**: Accepted

**Date**: 2025-01-08

**Decision Makers**: Development Team

## Context

The original settings screen implementation required manual synchronization of configuration changes across three separate locations:

1. **Config object (NVS storage)** - Persistent configuration data
2. **UI slider values** - Visual feedback in SettingsScreen
3. **PomodoroSequence object** - Runtime behavior of timer sequence

This created a maintenance burden where every settings change required developers to update all three components in sync. For example, when updating work duration:

```cpp
void SettingsScreen::onWorkDurationChange(uint16_t value) {
    // 1. Update Config (NVS storage)
    auto pomodoro = config_.getPomodoro();
    pomodoro.work_duration_min = value;
    config_.setPomodoro(pomodoro);

    // 2. Update UI slider (visual feedback)
    slider_work_duration_.setValue(value);

    // 3. Update PomodoroSequence (runtime behavior)
    sequence_.setWorkDuration(value);  // Easy to forget!
}
```

This pattern violated the **Single Responsibility Principle** because:
- SettingsScreen had direct knowledge of PomodoroSequence internals
- Changes to Config schema required updates in multiple locations
- No clear "source of truth" for configuration state
- High cognitive load for developers (must remember 3 update points)

### Problem Examples

The codebase had **13 manual synchronization points** across:
- `onSessionsChange()` - 1 sequence update
- `onCyclesChange()` - 1 sequence update
- `onModeClassic()` - 5 sequence updates (work/short/long/sessions/cycles)
- `onModeStudy()` - 5 sequence updates
- `onModeCustom()` - 3 sequence updates

Each of these was a potential source of bugs if synchronization was forgotten.

### Options Considered

1. **Continue with manual synchronization (status quo)**
   - Pros: No code changes required, existing patterns work
   - Cons: High maintenance burden, easy to introduce bugs, violates SRP

2. **Make PomodoroSequence the source of truth**
   - Pros: Direct updates to runtime behavior
   - Cons: Sequence would need to manage persistence, violates separation of concerns (sequence should track state, not configuration)

3. **Make Config the source of truth with callback pattern**
   - Pros: Single source of truth, unidirectional data flow, clear separation of concerns
   - Cons: Requires callback infrastructure, indirect updates

## Decision

**We will use Config (NVS storage) as the single source of truth for all configuration, with callback-based synchronization to PomodoroSequence.**

### Architecture

```
User Changes Setting
        ↓
SettingsScreen Handler
        ├─→ Update Config (NVS)          ← Single source of truth
        ├─→ Update UI Slider (visual)
        └─→ config_changed_callback_()
                    ↓
            ScreenManager::reloadTimerConfig()
                    ↓
            Config → PomodoroSequence      ← Automatic sync
```

### Rationale

1. **Single Responsibility**
   - SettingsScreen only manages UI and Config updates
   - ScreenManager manages coordination between Config and PomodoroSequence
   - PomodoroSequence only manages timer state and sequence logic

2. **Single Source of Truth**
   - Config (NVS) is authoritative for all configuration
   - PomodoroSequence is derived state, loaded from Config
   - No ambiguity about "current" configuration

3. **Unidirectional Data Flow**
   - Config → ScreenManager → PomodoroSequence (one direction)
   - No circular dependencies or bidirectional updates

4. **Reduced Coupling**
   - SettingsScreen no longer depends on PomodoroSequence
   - Changes to PomodoroSequence API don't affect SettingsScreen
   - Easier to test each component independently

5. **Maintainability**
   - Single point of update: `ScreenManager::reloadTimerConfig()`
   - Adding new settings only requires Config update + UI widget
   - No manual synchronization to remember

### Key Implementation Changes

**SettingsScreen.h**:
```cpp
// REMOVED: PomodoroSequence& dependency
// PomodoroSequence& sequence_;

// ADDED: Callback for config changes
using ConfigChangedCallback = std::function<void()>;
ConfigChangedCallback config_changed_callback_;

// Constructor change
SettingsScreen(Config& config,
               NavigationCallback navigate_callback,
               ConfigChangedCallback config_changed_callback);
```

**SettingsScreen.cpp - Before (13 manual sync points)**:
```cpp
void SettingsScreen::onWorkDurationChange(uint16_t value) {
    auto pomodoro = config_.getPomodoro();
    pomodoro.work_duration_min = value;
    config_.setPomodoro(pomodoro);
    sequence_.setWorkDuration(value);  // Manual sync point #1
}

void SettingsScreen::onModeClassic() {
    // ... update Config ...
    // Manual sync points #2-6
    sequence_.setWorkDuration(25);
    sequence_.setShortBreakDuration(5);
    sequence_.setLongBreakDuration(15);
    sequence_.setSessionsBeforeLong(4);
    sequence_.setNumCycles(1);
}
```

**SettingsScreen.cpp - After (single callback)**:
```cpp
void SettingsScreen::onWorkDurationChange(uint16_t value) {
    auto pomodoro = config_.getPomodoro();
    pomodoro.work_duration_min = value;
    config_.setPomodoro(pomodoro);
    config_changed_callback_();  // Notify parent
}

void SettingsScreen::onModeClassic() {
    // ... update Config ...
    config_changed_callback_();  // Single point of sync
}
```

**ScreenManager.cpp - Wiring callback**:
```cpp
// Constructor initialization
settings_screen_(config,
                 [this](ScreenID screen) { this->navigate(screen); },
                 [this]() { this->reloadTimerConfig(); }),

// Single point of update (already existed)
void ScreenManager::reloadTimerConfig() {
    auto pomodoro_config = config_.getPomodoro();
    sequence_.setSessionsBeforeLong(pomodoro_config.sessions_before_long);
    sequence_.setWorkDuration(pomodoro_config.work_duration_min);
    sequence_.setShortBreakDuration(pomodoro_config.short_break_min);
    sequence_.setLongBreakDuration(pomodoro_config.long_break_min);
}
```

### Migration Effort

- **Affected files**: 3 files (SettingsScreen.h/cpp, ScreenManager.cpp)
- **Removed code**: 13 manual `sequence_.*` calls
- **Added code**: 1 callback type, 1 callback member, callback wiring in constructor
- **Risk**: Low (compile-time safety, no runtime behavior change)
- **Estimated effort**: 1 hour

## Consequences

### Positive

- **Reduced Cognitive Load**: Developers only update Config, sync is automatic
- **Single Point of Update**: All sequence updates go through `ScreenManager::reloadTimerConfig()`
- **Testability**: SettingsScreen can be tested without PomodoroSequence dependency
- **Maintainability**: Adding new settings requires fewer code changes
- **Clear Ownership**: Config owns configuration, PomodoroSequence owns runtime state
- **No Synchronization Bugs**: Impossible to forget manual sync (compile error if callback not wired)

### Negative

- **Indirection**: Settings changes go through callback instead of direct update (minimal overhead)
- **Callback Overhead**: Function call overhead for config changes (negligible, ~10ns per call)
- **Learning Curve**: Developers need to understand callback pattern (mitigated by clear architecture)

### Neutral

- **Code Volume**: Slightly more code for callback infrastructure, but clearer separation
- **Performance**: No measurable impact (config changes are infrequent, <1 per second)

## Implementation

### Dependencies

No new dependencies required. Uses standard C++ `std::function` from `<functional>`.

### Testing Checklist

- [x] Verify Classic mode button updates Config and PomodoroSequence
- [x] Verify Study mode button updates Config and PomodoroSequence
- [x] Verify Custom mode button updates Config and PomodoroSequence
- [x] Verify slider changes trigger config reload
- [x] Verify navigation from Settings → Main triggers reload (MP-50)
- [ ] Unit test: SettingsScreen with mock Config (no PomodoroSequence dependency)
- [ ] Integration test: Full settings change flow with device

### Future Improvements

1. **Batching**: If multiple settings change at once (e.g., mode preset), could batch reload
2. **Change Detection**: Only reload sequence if relevant config changed (optimization)
3. **Validation**: Add config validation in ScreenManager before applying to sequence

## References

- **Single Responsibility Principle**: https://en.wikipedia.org/wiki/Single-responsibility_principle
- **Callback Pattern**: https://refactoring.guru/design-patterns/observer
- **Unidirectional Data Flow**: https://redux.js.org/understanding/thinking-in-redux/three-principles

## Related ADRs

- ADR-006: NVS Statistics Storage (Config as persistent storage)
- ADR-007: Classic Pomodoro Sequence Implementation (PomodoroSequence design)

## Revision History

- 2025-01-08: Initial version, refactor to callback-based architecture
