# M5 Pomodoro v2 - Development Guidelines

## Project Overview
M5Stack Core2 Pomodoro timer with gyro gestures, statistics tracking, and battery optimization.

**YouTrack Project**: MP (M5-POMODORO)
**Location**: `C:\HOME\1.SCRIPTS\021.M5_pomodoro_v2\`
**Platform**: M5Stack Core2 (ESP32), PlatformIO, M5Unified library

---

## Task Tracking with YouTrack

### Task Management Rules

1. **Always use YouTrack** for all M5 Pomodoro v2 tasks (project code: MP)
2. **Assign tasks to yourself** before starting work:
   ```
   update_issue("MP-X", customFields: {"Assignee": "claude"})
   ```
3. **Update task status** as you work:
   - Start work: `{"State": "In Progress"}`
   - Complete: `{"State": "Fixed"}` (after implementation)
   - Verified: `{"State": "Verified"}` (after testing)
   - Blocked: Add comment explaining blocker, keep state as "In Progress"
4. **Track time spent** (optional but recommended for significant work >1h):
   ```
   log_work("MP-X", durationMinutes: 120, workType: "Development")
   ```
5. **Link related issues** for dependencies:
   ```
   link_issues("MP-11", "depends on", "MP-10")  // MP-11 depends on MP-10
   ```
6. **Update descriptions** with implementation details:
   - File paths created (e.g., `src/core/TimerStateMachine.h/cpp`)
   - Class names and key methods
   - Important decisions made during implementation
   - Blockers encountered and resolved

---

## Task Workflow

```
Submitted → (assign to claude) → In Progress → (complete work) → Fixed → (test) → Verified
```

**State Meanings**:
- **Submitted**: New task, not started
- **In Progress**: Currently being worked on (assign to yourself)
- **Fixed**: Implementation complete, ready for testing
- **Verified**: Tested and confirmed working
- **Blocked**: Cannot proceed due to dependency

---

## Project Structure

### Development Phases

**Epic MP-1**: M5 Pomodoro v2 - Complete Rebuild

#### Phase 1: Documentation & Planning (Week 1) - CURRENT PHASE
- **MP-2**: Create comprehensive planning documents ✅ 2/10 complete
  - ✅ `docs/00-PROJECT-OVERVIEW.md`
  - ✅ `docs/01-TECHNICAL-REQUIREMENTS.md`
  - ⏳ `docs/02-ARCHITECTURE.md`
  - ⏳ `docs/03-STATE-MACHINE.md`
  - ⏳ `docs/04-GYRO-CONTROL.md`
  - ⏳ `docs/05-STATISTICS-SYSTEM.md`
  - ⏳ `docs/06-POWER-MANAGEMENT.md`
  - ⏳ `docs/07-UI-DESIGN.md`
  - ⏳ `docs/08-CLOUD-SYNC.md`
  - ⏳ `docs/09-BUILD-DEPLOY.md`
- **MP-3**: Create ADRs for key decisions (8 ADRs)
  - `docs/ADRs/ADR-001-m5unified-library.md`
  - `docs/ADRs/ADR-002-dual-core-architecture.md`
  - `docs/ADRs/ADR-003-freertos-synchronization.md`
  - `docs/ADRs/ADR-004-gyro-polling-strategy.md`
  - `docs/ADRs/ADR-005-light-sleep-during-timer.md`
  - `docs/ADRs/ADR-006-nvs-statistics-storage.md`
  - `docs/ADRs/ADR-007-classic-pomodoro-sequence.md`
  - `docs/ADRs/ADR-008-power-mode-design.md`

#### Phase 2: Local Timer (Week 2-7)
- **MP-4**: Initialize project structure (PlatformIO, Git, folder structure)
- **MP-5**: Verify M5Unified library and dependencies
- **MP-6**: Create core module structure (src/ folders)
- **MP-7**: Hardware abstraction layer (5 classes)
- **MP-8**: Gyro controller - rotation detection
- **MP-9**: Power manager - AXP192 integration
- **MP-10**: Pomodoro sequence tracking (classic/study modes)
- **MP-11**: Timer state machine (5 states, 6 events)
- **MP-12**: Unit tests for state machine
- **MP-13**: Time manager - NTP and RTC sync
- **MP-14**: Statistics tracking system (90-day NVS storage)
- **MP-15**: UI rendering infrastructure (dirty rectangles, sprites)
- **MP-16**: UI widget library (5 widgets)
- **MP-17**: Main screen implementation
- **MP-18**: Statistics screen
- **MP-19**: Settings screen (15+ options)
- **MP-20**: Pause screen
- **MP-21**: Screen manager and navigation
- **MP-22**: Audio system implementation
- **MP-23**: LED animations
- **MP-24**: Gyro gesture integration with state machine
- **MP-25**: Touch input handling
- **MP-26**: HMI encoder input handling
- **MP-27**: Haptic feedback implementation
- **MP-28**: Configuration management and persistence
- **MP-29**: Power mode implementation (3 modes)
- **MP-30**: Sleep mode implementation (light + deep)
- **MP-31**: Battery life testing
- **MP-32**: Local integration testing (8 test cases)
- **MP-33**: Bug fixes from local testing

#### Phase 3: Cloud Integration (Week 7-8)
- **MP-34**: Network infrastructure - WiFi and Core 1 task
- **MP-35**: MQTT client implementation
- **MP-36**: AWS IoT Device Shadow sync
- **MP-37**: Adaptive sync strategy (power-aware)
- **MP-38**: Cloud integration testing

#### Phase 4: Polish & Testing (Week 8)
- **MP-39**: Final integration testing (7-day uptime)
- **MP-40**: User documentation
- **MP-41**: Code cleanup and review
- **MP-42**: Project README and documentation index

---

## Key Task Dependencies

### Documentation Phase
```
MP-2 (planning docs) ────┐
                          ├──→ MP-4 (project setup)
MP-3 (ADRs) ─────────────┘
```

### Core Infrastructure
```
MP-4 (project setup) → MP-5 (M5Unified verify) → MP-7 (hardware abstraction)
                     → MP-6 (module structure) → MP-10 (sequence) → MP-11 (state machine)
```

### UI Development
```
MP-15 (rendering) → MP-16 (widgets) → MP-17 (main screen)
                                    → MP-18 (stats screen)
                                    → MP-19 (settings)
                                    → MP-20 (pause)
                                    → MP-21 (screen manager)
```

### Integration
```
MP-32 (local test) → MP-34 (network) → MP-35 (MQTT) → MP-36 (shadow) → MP-38 (cloud test) → MP-39 (final test)
```

---

## File Organization

### Source Code Structure
```
src/
├── core/
│   ├── Config.h/cpp                    # NVS configuration storage (MP-28)
│   ├── PomodoroSequence.h/cpp          # Classic/study sequence logic (MP-10)
│   ├── TimerStateMachine.h/cpp         # 5 states, 6 events (MP-11)
│   ├── Statistics.h/cpp                # 90-day stats, NVS storage (MP-14)
│   └── TimeManager.h/cpp               # NTP sync, RTC, drift learning (MP-13)
├── hardware/
│   ├── AudioPlayer.h/cpp               # I2S WAV playback (MP-22)
│   ├── GyroController.h/cpp            # MPU6886 rotation detection (MP-8)
│   ├── HMIController.h/cpp             # Encoder + buttons I2C (MP-26)
│   ├── LEDController.h/cpp             # FastLED SK6812 control (MP-23)
│   └── PowerManager.h/cpp              # AXP192 battery, sleep modes (MP-9)
├── ui/
│   ├── ScreenManager.h/cpp             # Screen routing, navigation (MP-21)
│   ├── Renderer.h/cpp                  # Canvas, dirty rectangles (MP-15)
│   ├── screens/
│   │   ├── MainScreen.h/cpp            # Primary timer display (MP-17)
│   │   ├── StatsScreen.h/cpp           # Weekly bar chart (MP-18)
│   │   ├── SettingsScreen.h/cpp        # 15+ config options (MP-19)
│   │   └── PauseScreen.h/cpp           # Paused state UI (MP-20)
│   └── widgets/
│       ├── ProgressBar.h/cpp           # Timer progress bar (MP-16)
│       ├── StatusBar.h/cpp             # WiFi, battery, time (MP-16)
│       ├── SequenceIndicator.h/cpp     # ●●○○ session dots (MP-16)
│       └── StatsChart.h/cpp            # Weekly bar chart (MP-16)
├── network/
│   ├── WiFiManager.h/cpp               # Connection, power-aware (MP-34)
│   ├── MQTTClient.h/cpp                # AWS IoT MQTT TLS (MP-35)
│   └── ShadowSync.h/cpp                # Device Shadow protocol (MP-36)
└── main.cpp                            # Init only (<100 lines)
```

### Documentation Structure
```
docs/
├── 00-PROJECT-OVERVIEW.md              # ✅ Goals, constraints, success metrics
├── 01-TECHNICAL-REQUIREMENTS.md        # ✅ Functional & non-functional requirements
├── 02-ARCHITECTURE.md                  # ⏳ Component diagram, data flows
├── 03-STATE-MACHINE.md                 # ⏳ States, transitions, sequence logic
├── 04-GYRO-CONTROL.md                  # ⏳ MPU6886 polling, gesture detection
├── 05-STATISTICS-SYSTEM.md             # ⏳ Data schema, NVS storage
├── 06-POWER-MANAGEMENT.md              # ⏳ AXP192, power modes, battery
├── 07-UI-DESIGN.md                     # ⏳ Screen layouts, color palette
├── 08-CLOUD-SYNC.md                    # ⏳ AWS IoT shadow protocol
├── 09-BUILD-DEPLOY.md                  # ⏳ PlatformIO config, dependencies
└── ADRs/
    ├── ADR-001-m5unified-library.md
    ├── ADR-002-dual-core-architecture.md
    ├── ADR-003-freertos-synchronization.md
    ├── ADR-004-gyro-polling-strategy.md
    ├── ADR-005-light-sleep-during-timer.md
    ├── ADR-006-nvs-statistics-storage.md
    ├── ADR-007-classic-pomodoro-sequence.md
    └── ADR-008-power-mode-design.md
```

---

## Task Query Examples

### Finding Tasks to Work On
```python
# Available tasks not yet started
search_issues("project: MP State: Submitted Assignee: Unassigned")

# Your active tasks
search_issues("project: MP State: {In Progress} Assignee: claude")

# Critical backlog items
search_issues("project: MP Priority: Critical State: Submitted")

# Tasks blocked by MP-2 (documentation)
search_issues("project: MP depends on: MP-2")

# All your unresolved tasks
search_issues("project: MP #Unresolved Assignee: claude")

# Tasks in current phase (documentation)
search_issues("project: MP summary: documentation OR summary: ADR")
```

---

## Task Completion Checklist

Before marking task as "Fixed":
1. ✅ **Implementation complete** - All acceptance criteria met
2. ✅ **Files created/updated** - As documented in task description
3. ✅ **Code quality** - No files >300 lines, follows project style
4. ✅ **Related tasks updated** - Dependencies, blockers linked
5. ✅ **Task description updated** - Actual file paths, class names, decisions documented
6. ✅ **Time logged** - If significant work (>1 hour)
7. ✅ **Tested** - Basic functionality verified (if applicable)

---

## Example Task Update Flow

### Starting a Task
```python
# 1. Find available task
search_issues("project: MP State: Submitted Priority: Critical")

# 2. Assign to yourself and mark in progress
update_issue("MP-4", customFields: {"Assignee": "claude", "State": "In Progress"})

# 3. Work on the task...
# (create files, write code, test)
```

### During Development
```python
# 4. Update description with implementation details
update_issue("MP-4", description: """
Setup PlatformIO project with folder structure:

**Implementation Details**:
- Created `platformio.ini` with M5Unified library
- Setup folder structure: src/, test/, docs/, config/
- Created `.gitignore` excluding secrets.h, .pio/, build/
- Created `config/secrets.h.template` for WiFi + AWS IoT credentials
- Initialized Git repository with initial commit
- Added entry to `C:\HOME\1.Scripts\descript.ion`

**Dependencies Verified**:
- M5Unified v0.1.12 (latest stable from Context7)

**Blocks**: MP-5, MP-6 can now proceed
""")
```

### Completing a Task
```python
# 5. Mark as Fixed
update_issue("MP-4", customFields: {"State": "Fixed"})

# 6. Log time spent
log_work("MP-4", durationMinutes: 120, workType: "Implementation")

# 7. Update blocked tasks (if any)
# Add comment to MP-5: "MP-4 complete, you can proceed"
```

---

## Development Best Practices

### Code Style
- **No files >300 lines** (split into logical modules)
- **Descriptive names**: `PomodoroSequence` not `PomSeq`
- **Header guards**: `#ifndef TIMER_STATE_MACHINE_H`
- **Type hints**: Use enums (`PomodoroState`, `PomodoroMode`)
- **Comments**: Explain WHY, not WHAT

### Hardware Constraints
- **SRAM**: 520KB (budget: 200KB max for code)
- **PSRAM**: 8MB (use for canvas buffer, sprite cache)
- **Flash**: 16MB (budget: 12MB max)
- **MPU6886 interrupt**: NOT wired to GPIO (polling only, cannot wake from deep sleep)

### FreeRTOS Guidelines
- **Core 0**: UI rendering, touch/gyro input, timer state machine
- **Core 1**: WiFi, MQTT, NTP synchronization
- **Queues**: Inter-core messaging (Core 1 → Core 0)
- **Mutexes**: Protect shared resources (I2C bus, shadow state)
- **Stack size**: 10KB for network task, default for main loop

### Testing Approach
- **Unit tests**: State machine, sequence logic, statistics (70% coverage target)
- **Integration tests**: Complete 4-session cycle, gyro gestures, battery drain
- **Hardware tests**: Actual device required (no emulator)

---

## Common Issues & Solutions

### Issue: Task blocked by dependency
**Solution**:
1. Check dependency status: `get_issue("MP-X")`
2. If dependency not started, ping user or start it yourself
3. Add comment to your task explaining blocker
4. Work on unblocked task meanwhile

### Issue: Implementation differs from plan
**Solution**:
1. Update task description with actual implementation
2. If significantly different, create new ADR documenting decision
3. Update related tasks if their assumptions changed

### Issue: Found bug during implementation
**Solution**:
1. Create new task for bug: `create_issue("MP", summary: "Bug: ...")`
2. Link to parent task: `link_issues("MP-NEW", "relates to", "MP-PARENT")`
3. Fix bug, mark both tasks Fixed

### Issue: Need to split large task
**Solution**:
1. Keep original task as parent
2. Create subtasks for each piece
3. Link subtasks to parent: use `parentIssue` parameter in `create_issue`

---

## Current Status

**Phase**: 1 - Documentation & Planning
**Active Task**: MP-2 (Create comprehensive planning documents)
**Progress**: 2 of 10 planning docs complete
**Next**: Complete remaining 8 docs + 8 ADRs, then move to MP-4 (project setup)

**Completed**:
- ✅ MP-1: Epic created with 42 subtasks
- ✅ `docs/00-PROJECT-OVERVIEW.md`
- ✅ `docs/01-TECHNICAL-REQUIREMENTS.md`

**In Progress**:
- ⏳ MP-2: Remaining planning documents (02-09)
- ⏳ MP-3: 8 ADRs

**Blocked**: All Phase 2+ tasks wait for MP-2, MP-3 completion

---

## Quick Reference

### M5Unified Key APIs
```cpp
M5.begin();              // Initialize hardware
M5.update();             // Poll touch, buttons (call every loop)
M5.Display.drawString(); // Draw text
M5.Touch.getDetail();    // Get touch events
M5.Imu.getAccelData();   // Read accelerometer
M5.Power.getBatteryLevel(); // Battery %
```

### NVS Storage
```cpp
preferences.begin("namespace", false); // R/W mode
preferences.putUInt("key", value);
uint32_t val = preferences.getUInt("key", default);
preferences.end();
```

### FreeRTOS
```cpp
xTaskCreatePinnedToCore(task, "name", stack, params, priority, handle, core);
xQueueSend(queue, &data, timeout);
xQueueReceive(queue, &buffer, timeout);
xSemaphoreTake(mutex, portMAX_DELAY);
xSemaphoreGive(mutex);
```

---

**Last Updated**: 2025-01-03
**Document Version**: 1.0
**Project Phase**: Documentation & Planning
