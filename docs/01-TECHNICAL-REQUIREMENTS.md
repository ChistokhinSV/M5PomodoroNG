# Technical Requirements - M5 Pomodoro v2

## Document Information
- **Version**: 1.0
- **Date**: 2025-01-03
- **Status**: Draft
- **Author**: Development Team

---

## 1. Functional Requirements

### 1.1 Timer Functionality

#### FR-1.1.1 Classic Pomodoro Sequence
**Priority**: MUST HAVE
**Description**: Support standard Pomodoro technique with 4-session cycle

**Requirements**:
- System SHALL support classic sequence: 25min work → 5min rest (×3) → 25min work → 15min long rest
- System SHALL track current session number (1/4, 2/4, 3/4, 4/4)
- System SHALL automatically transition to short rest after sessions 1-3
- System SHALL automatically transition to long rest after session 4
- System SHALL reset sequence to session 1 after long rest completion
- System SHALL reset sequence at midnight (00:00 local time)
- User SHALL be able to manually reset sequence

**Acceptance Criteria**:
- Complete 4-session cycle transitions correctly without manual intervention
- Session counter displays accurately on UI
- Midnight rollover triggers sequence reset

#### FR-1.1.2 Study Mode
**Priority**: MUST HAVE
**Description**: Extended work/rest cycle for longer focus sessions

**Requirements**:
- System SHALL support study mode: 45min work → 15min rest (repeating)
- Study mode SHALL NOT track session numbers (continuous cycle)
- System SHALL allow switching between Classic and Study modes
- Mode switch SHALL preserve current timer state if running

**Acceptance Criteria**:
- 45-15 cycle repeats indefinitely
- Mode selection visible on UI
- Switch mid-timer preserves remaining time

#### FR-1.1.3 Custom Durations
**Priority**: MUST HAVE
**Description**: User-configurable work and rest periods

**Requirements**:
- User SHALL configure work duration: 5 to 90 minutes (5-min increments)
- User SHALL configure short rest: 1 to 10 minutes (1-min increments)
- User SHALL configure long rest: 10 to 30 minutes (5-min increments)
- User SHALL configure study rest: 10 to 30 minutes (5-min increments)
- Configurations SHALL persist across reboots (NVS storage)
- Default values: 25min work, 5min short rest, 15min long rest, 45min study work

**Acceptance Criteria**:
- Encoder adjusts durations in specified increments
- Settings persist after power cycle
- All durations work correctly in timer

#### FR-1.1.4 Pause and Resume
**Priority**: MUST HAVE
**Description**: Ability to pause timer without losing progress

**Requirements**:
- User SHALL pause running timer via touch button, encoder button, or gyro gesture
- System SHALL preserve exact remaining time when paused
- System SHALL display "PAUSED" state clearly on UI
- User SHALL resume from pause via touch button or gyro gesture (lay flat)
- System SHALL NOT count time while paused
- Paused time SHALL persist across screen sleep (not deep sleep)

**Acceptance Criteria**:
- Remaining time frozen during pause
- Resume continues from exact pause point
- Multiple pause/resume cycles work correctly

#### FR-1.1.5 Auto-Start Options
**Priority**: SHOULD HAVE
**Description**: Configurable automatic transitions

**Requirements**:
- User SHALL enable/disable auto-start for rest periods
- User SHALL enable/disable auto-start for next work session
- If disabled, system SHALL wait for user START command
- Settings SHALL persist in NVS

**Acceptance Criteria**:
- Auto-start off: timer stops, waits for user action
- Auto-start on: timer transitions automatically after 3-second countdown

---

### 1.2 Gyro Gesture Control

#### FR-1.2.1 Rotation Detection
**Priority**: MUST HAVE
**Description**: Start/pause timer by rotating device to edge

**Requirements**:
- System SHALL detect rotation ≥60° from vertical (configurable 30-90°)
- Rotation SHALL trigger START when in STOPPED state
- Rotation SHALL trigger PAUSE when in POMODORO or REST state
- System SHALL debounce gestures with 500ms cooldown
- Detection SHALL work in both directions (clockwise/counterclockwise)
- System SHALL provide visual + haptic feedback on detection

**Acceptance Criteria**:
- 60° rotation consistently triggers action
- No false triggers during normal handling
- Works in all device orientations

#### FR-1.2.2 Flat Detection
**Priority**: MUST HAVE
**Description**: Start next session by laying device flat (screen up)

**Requirements**:
- System SHALL detect horizontal position (±10° tolerance)
- Flat detection SHALL trigger "start next session" when in REST state
- Flat detection SHALL trigger RESUME when in PAUSED state
- System SHALL ignore flat detection during active POMODORO
- Gesture SHALL require 1-second hold to prevent accidental triggers

**Acceptance Criteria**:
- Laying flat on table after rest starts next pomodoro
- Quick tilt/return doesn't trigger (1s hold requirement)
- Works on flat surfaces (desk, table, lap)

#### FR-1.2.3 Emergency Stop
**Priority**: SHOULD HAVE
**Description**: Immediate timer stop via shake gesture

**Requirements**:
- System SHALL detect double-shake gesture (two rapid shakes within 1 second)
- Double-shake SHALL immediately STOP timer from any state
- System SHALL provide strong haptic + visual feedback
- Gesture SHALL work regardless of device orientation

**Acceptance Criteria**:
- Double-shake stops timer from any state
- Requires deliberate motion (not accidental bumps)
- Visual confirmation displayed

#### FR-1.2.4 Gyro Configuration
**Priority**: MUST HAVE
**Description**: User control over gyro behavior

**Requirements**:
- User SHALL enable/disable gyro gestures globally
- User SHALL adjust rotation sensitivity (30° to 90° threshold)
- User SHALL see current orientation on settings screen (calibration aid)
- Settings SHALL persist in NVS
- When disabled, only touch/encoder input works

**Acceptance Criteria**:
- Gyro disable prevents all gesture detection
- Sensitivity adjustment affects rotation threshold
- Setting persists across reboots

---

### 1.3 Statistics Tracking

#### FR-1.3.1 Daily Tracking
**Priority**: MUST HAVE
**Description**: Count completed pomodoros per day

**Requirements**:
- System SHALL increment daily counter on pomodoro completion (not on stop)
- System SHALL store daily stats in NVS (key: `stats_YYYYMMDD`)
- System SHALL track: pomodoros_completed, total_work_minutes, interruptions, session_date
- System SHALL handle midnight rollover correctly
- System SHALL display today's count on main screen (badge)

**Acceptance Criteria**:
- Counter increments only on completion (timer reaches 00:00)
- Stops mid-session don't count as completed
- Midnight rollover creates new day entry

#### FR-1.3.2 Streak Calculation
**Priority**: MUST HAVE
**Description**: Track consecutive days with productivity

**Requirements**:
- System SHALL calculate streak: consecutive days with ≥1 completed pomodoro
- System SHALL track current_streak_days and best_streak_days
- System SHALL break streak if zero pomodoros completed in a day
- System SHALL persist streak data in NVS
- System SHALL display current streak on statistics screen

**Acceptance Criteria**:
- Streak increments daily with ≥1 completion
- Streak resets to 0 after day with 0 completions
- Best streak persists even after current streak breaks

#### FR-1.3.3 History and Retention
**Priority**: MUST HAVE
**Description**: 90-day rolling window of statistics

**Requirements**:
- System SHALL store 90 days of daily statistics (circular buffer)
- System SHALL delete oldest day when adding day 91
- System SHALL calculate weekly aggregate (Mon-Sun, current week)
- System SHALL track lifetime_pomodoros (all-time counter, separate key)
- All stats SHALL persist in NVS

**Acceptance Criteria**:
- 90-day buffer maintains newest 90 days
- Weekly chart displays current week correctly
- Lifetime counter never decreases

#### FR-1.3.4 Completion Rate
**Priority**: SHOULD HAVE
**Description**: Track session success rate

**Requirements**:
- System SHALL track pomodoros_started (incremented on START)
- System SHALL track pomodoros_completed (incremented on expiration)
- System SHALL calculate completion_rate = completed / started × 100%
- System SHALL display rate on statistics screen

**Acceptance Criteria**:
- Rate calculation accurate (handle division by zero)
- Started count includes all STARTs (even if stopped)
- Completed count only includes finished sessions

---

### 1.4 User Interface

#### FR-1.4.1 Main Screen
**Priority**: MUST HAVE
**Description**: Primary timer display

**Requirements**:
- SHALL display: status bar (WiFi, battery, time), sequence indicator (●●○○), large timer (MM:SS), progress bar, task name, touch buttons
- Timer SHALL use 96pt font for visibility
- Progress bar SHALL fill based on elapsed percentage
- Sequence indicator SHALL highlight completed sessions
- Touch buttons: [Start], [Pause], [Stop], [Stats], [Settings]
- Update rate: 1 second for timer, 100ms for UI elements

**Acceptance Criteria**:
- All elements visible without scrolling
- Timer readable from 1 meter distance
- Touch buttons respond <100ms

#### FR-1.4.2 Statistics Screen
**Priority**: MUST HAVE
**Description**: Visualize productivity history

**Requirements**:
- SHALL display: header with [Back] button, today summary (count + streak), weekly bar chart (Mon-Sun), totals (lifetime + average)
- Bar chart SHALL use relative scaling (tallest bar = 100% height)
- SHALL handle week with zero pomodoros (empty bars)
- SHALL update in real-time if stats change

**Acceptance Criteria**:
- Weekly chart displays 7 bars correctly
- Today and streak values accurate
- Navigation back to main screen works

#### FR-1.4.3 Settings Screen
**Priority**: MUST HAVE
**Description**: Configuration interface

**Requirements**:
- SHALL provide options for: timer durations (work, short rest, long rest), mode selection (Classic/Study), audio volume (0-100%), LED brightness (0-100%), screen brightness (0-100%), gyro sensitivity (30-90°), gyro enable/disable, power mode (Performance/Balanced/Battery Saver), timezone offset (-12 to +12), auto-start options, sleep timeout (0-600s)
- Encoder SHALL scroll options + adjust values
- Touch SHALL tap to select
- Changes SHALL persist immediately to NVS
- SHALL display current values for all settings

**Acceptance Criteria**:
- All 15+ settings accessible
- Value changes take effect immediately
- Settings persist across reboot

#### FR-1.4.4 Pause Screen
**Priority**: SHOULD HAVE
**Description**: Paused state visualization

**Requirements**:
- SHALL display: amber-tinted background, large "PAUSED" label, frozen timer (remaining time), [Resume] and [Stop] buttons
- LED SHALL pulse amber
- SHALL disable auto-sleep while paused

**Acceptance Criteria**:
- Paused state clearly distinguishable from running
- Resume/stop buttons respond correctly

---

### 1.5 Feedback Systems

#### FR-1.5.1 Audio Alerts
**Priority**: MUST HAVE
**Description**: Sound feedback for events

**Requirements**:
- System SHALL play WAV files via I2S (NS4168 amplifier)
- SHALL provide sounds: start.wav (work start), rest.wav (rest start), long_rest.wav (long rest start), warning.wav (30s before end)
- Volume SHALL be configurable (0-100%)
- User SHALL mute audio via settings
- Sounds SHALL play even during screen sleep

**Acceptance Criteria**:
- All 4 sounds distinct and audible
- Volume control works across full range
- Mute disables all sounds

#### FR-1.5.2 LED Animations
**Priority**: MUST HAVE
**Description**: Visual ambient feedback

**Requirements**:
- System SHALL control 10 SK6812 LEDs (FastLED)
- SHALL provide patterns: breathe (pomodoro, green), fade (rest, blue), blink (paused, amber), flash (warning, yellow), rainbow (milestone)
- Brightness SHALL be configurable (0-100%)
- Brightness SHALL auto-adjust per power mode
- User SHALL disable LEDs via settings

**Acceptance Criteria**:
- Each state has distinct LED pattern
- Brightness control smooth (no flicker)
- Disable turns off all LEDs

#### FR-1.5.3 Haptic Feedback
**Priority**: SHOULD HAVE
**Description**: Vibration motor feedback

**Requirements**:
- System SHALL vibrate on: button press (50ms), timer complete (3× 200ms), gesture detect (100ms), error (2× 50ms)
- User SHALL enable/disable haptic via settings
- Haptic SHALL work even during screen sleep

**Acceptance Criteria**:
- All 4 patterns distinguishable
- Disable prevents all vibrations

---

### 1.6 Power Management

#### FR-1.6.1 Power Modes
**Priority**: MUST HAVE
**Description**: Three-tier battery optimization

**Requirements**:
- System SHALL provide modes: PERFORMANCE (100% screen, 100Hz gyro, WiFi always, 10 FPS), BALANCED (80% screen, 50Hz gyro, WiFi periodic, 10 FPS, default), BATTERY_SAVER (50% screen, 25Hz gyro, WiFi on-change, 4 FPS)
- System SHALL auto-switch to BATTERY_SAVER when battery <20%
- System SHALL allow manual mode override in settings
- System SHALL restore auto mode on charging

**Acceptance Criteria**:
- Each mode applies settings correctly
- Auto-switch triggers at 20% threshold
- Manual override persists until changed

#### FR-1.6.2 Sleep Modes
**Priority**: MUST HAVE
**Description**: Aggressive power saving

**Requirements**:
- System SHALL use light sleep during active timer (CPU idle between ticks, peripherals on)
- System SHALL use deep sleep after inactivity timeout (configurable 0-600s, default 300s)
- Deep sleep SHALL save timer state to RTC memory
- System SHALL wake from: touch screen, RTC timer (periodic check every 60s)
- System SHALL restore timer state on wake
- User SHALL disable sleep via settings (timeout = 0)

**Acceptance Criteria**:
- Light sleep maintains gyro responsiveness
- Deep sleep current <1mA (measured)
- Wake from touch works reliably
- Timer resumes correctly after wake

#### FR-1.6.3 Battery Monitoring
**Priority**: MUST HAVE
**Description**: Display and track battery status

**Requirements**:
- System SHALL read battery voltage via AXP192
- System SHALL calculate battery percentage (non-linear curve)
- System SHALL display battery % + icon on status bar
- System SHALL show charging indicator when USB connected
- System SHALL update battery reading every 30 seconds

**Acceptance Criteria**:
- Battery % accurate within ±5%
- Charging icon appears when connected
- Low battery (<10%) shows warning

---

### 1.7 Cloud Synchronization

#### FR-1.7.1 AWS IoT Device Shadow
**Priority**: MUST HAVE
**Description**: State synchronization via MQTT

**Requirements**:
- System SHALL connect to AWS IoT Core via MQTT over TLS 1.2
- System SHALL use Device Shadow protocol (desired/reported)
- System SHALL publish state on: timer START, PAUSE, STOP, mode change
- System SHALL subscribe to desired state for remote commands
- Shadow document SHALL use optimized schema (shortened keys)
- System SHALL queue up to 10 updates when offline
- System SHALL sync queue on reconnection

**Acceptance Criteria**:
- Shadow reported matches device state
- Remote commands (from phone app) apply correctly
- Offline queue prevents data loss

#### FR-1.7.2 Adaptive Sync Strategy
**Priority**: SHOULD HAVE
**Description**: Battery-aware sync intervals

**Requirements**:
- PERFORMANCE mode: report every 60s
- BALANCED mode: report every 120s
- BATTERY_SAVER mode: report on change only
- Battery <10%: disable periodic sync
- System SHALL maintain persistent MQTT connection (not reconnect per publish)

**Acceptance Criteria**:
- Sync interval matches power mode
- Low battery disables periodic reports
- Connection persists between publishes

#### FR-1.7.3 Integration Support
**Priority**: MUST HAVE
**Description**: Existing Lambda integrations

**Requirements**:
- System SHALL work with existing Toggl webhook Lambda
- System SHALL work with existing Google Calendar Lambda
- Shadow document SHALL include task description field
- System SHALL publish start_time + duration for integration

**Acceptance Criteria**:
- Toggl receives time entries correctly
- Google Calendar creates events
- Integration works without device changes (Lambda handles)

---

## 2. Non-Functional Requirements

### 2.1 Performance

#### NFR-2.1.1 UI Responsiveness
**Priority**: MUST HAVE

**Requirements**:
- UI SHALL render at ≥10 FPS (100ms frame time) during normal operation
- Touch input SHALL respond within 100ms
- Gyro gesture SHALL detect within 500ms
- Timer tick SHALL update UI within 50ms
- Screen transitions SHALL complete within 100ms (fade animation)

**Measurement**: Frame time profiling, input latency tests

#### NFR-2.1.2 Network Performance
**Priority**: MUST HAVE

**Requirements**:
- MQTT connection SHALL establish within 15 seconds (cold start with TLS handshake)
- MQTT reconnection SHALL complete within 2 seconds (with SSL session resume)
- Shadow publish SHALL complete within 500ms (when connected)
- NTP sync SHALL complete within 2 seconds (per server, 3 servers = 6s max)

**Measurement**: Network latency logging, reconnect timer

#### NFR-2.1.3 Battery Life
**Priority**: MUST HAVE

**Requirements**:
- BALANCED mode SHALL achieve ≥4 hours runtime on 390mAh battery (target, measure actual)
- PERFORMANCE mode SHALL achieve ≥2 hours runtime
- BATTERY_SAVER mode SHALL achieve ≥6 hours runtime
- Deep sleep SHALL draw <1mA current
- Light sleep SHALL draw <50mA current

**Measurement**: Battery test (100% → 0% in each mode), current meter readings

---

### 2.2 Reliability

#### NFR-2.2.1 Stability
**Priority**: MUST HAVE

**Requirements**:
- System SHALL run continuously for ≥7 days without crashes
- System SHALL handle ≥1000 timer start/stop cycles without errors
- System SHALL survive ≥100 WiFi disconnect/reconnect cycles
- System SHALL handle midnight rollover correctly (date change)

**Measurement**: Longevity test, stress test, rollover test

#### NFR-2.2.2 Data Integrity
**Priority**: MUST HAVE

**Requirements**:
- Statistics SHALL persist correctly across reboots (no data loss)
- Settings SHALL persist correctly across reboots
- Timer state SHALL restore correctly after deep sleep wake
- NVS writes SHALL be atomic (no partial writes)

**Measurement**: Reboot test (100 cycles), deep sleep test, NVS integrity check

#### NFR-2.2.3 Error Recovery
**Priority**: MUST HAVE

**Requirements**:
- System SHALL auto-reconnect WiFi on disconnect
- System SHALL auto-reconnect MQTT on disconnect
- System SHALL continue timer operation during network outage
- System SHALL retry failed NTP sync with exponential backoff
- System SHALL NOT crash on invalid cloud commands (validate input)

**Measurement**: Fault injection tests, network disconnect tests

---

### 2.3 Usability

#### NFR-2.3.1 Readability
**Priority**: MUST HAVE

**Requirements**:
- Timer SHALL be readable from 1 meter distance
- Text SHALL use high-contrast colors (white on dark)
- Icons SHALL be recognizable at 64×64 pixels
- Touch targets SHALL be ≥48×48 pixels (comfortable tap)

**Measurement**: User testing, accessibility guidelines

#### NFR-2.3.2 Learnability
**Priority**: SHOULD HAVE

**Requirements**:
- User SHALL complete first pomodoro within 5 minutes of setup
- User SHALL understand gyro gestures within 3 attempts
- Settings SHALL be self-explanatory (no manual required)
- Error messages SHALL be actionable

**Measurement**: First-use testing

#### NFR-2.3.3 Feedback
**Priority**: MUST HAVE

**Requirements**:
- Every user action SHALL provide immediate feedback (visual + haptic)
- Timer state SHALL be obvious at a glance (color coding)
- Errors SHALL display user-friendly messages (toast notifications)

**Measurement**: UX review, feedback delay testing

---

### 2.4 Maintainability

#### NFR-2.4.1 Code Quality
**Priority**: MUST HAVE

**Requirements**:
- No source file SHALL exceed 300 lines (excluding comments/blanks)
- All public functions SHALL have docstring comments
- All magic numbers SHALL be named constants
- Cyclomatic complexity SHALL NOT exceed 15 per function

**Measurement**: Static analysis, code review

#### NFR-2.4.2 Modularity
**Priority**: MUST HAVE

**Requirements**:
- System SHALL use component-based architecture (11 components)
- Components SHALL have clear interfaces (public methods only)
- Hardware dependencies SHALL be abstracted (HAL classes)
- Network code SHALL be isolated to Core 1 task

**Measurement**: Architecture review, dependency graph

#### NFR-2.4.3 Testability
**Priority**: SHOULD HAVE

**Requirements**:
- Core logic (state machine, sequence, statistics) SHALL achieve ≥70% unit test coverage
- Hardware abstraction SHALL support mocking (interfaces)
- Tests SHALL run in <10 seconds total

**Measurement**: Coverage report, test execution time

---

### 2.5 Scalability

#### NFR-2.5.1 Memory Efficiency
**Priority**: MUST HAVE

**Requirements**:
- SRAM usage SHALL NOT exceed 200KB (leave 320KB free for stack)
- PSRAM usage SHALL NOT exceed 2MB (leave 6MB buffer)
- Flash usage SHALL NOT exceed 12MB (leave 4MB for future updates)
- Heap fragmentation SHALL remain <20% after 24 hours

**Measurement**: Memory profiler, heap stats

#### NFR-2.5.2 Storage Efficiency
**Priority**: MUST HAVE

**Requirements**:
- NVS SHALL NOT exceed 10KB for config + 90-day stats
- WAV files SHALL be compressed (not raw PCM)
- Images SHALL use PNG compression
- Shadow document SHALL use shortened keys (<200 bytes)

**Measurement**: NVS dump, file size inspection

---

### 2.6 Security

#### NFR-2.6.1 Credential Management
**Priority**: MUST HAVE

**Requirements**:
- WiFi password SHALL NOT be logged
- AWS private key SHALL be stored in secrets.h (gitignored)
- Secrets SHALL NOT appear in compiled binary strings (if possible)
- Debug logs SHALL NOT expose sensitive data

**Measurement**: Code review, binary string dump

#### NFR-2.6.2 Network Security
**Priority**: MUST HAVE

**Requirements**:
- MQTT SHALL use TLS 1.2 (minimum)
- Device SHALL authenticate via X.509 certificate
- Device SHALL validate server certificate (Amazon Root CA)
- No plain HTTP requests (HTTPS only for NTP/web)

**Measurement**: Network traffic capture, TLS version check

---

### 2.7 Compatibility

#### NFR-2.7.1 Platform
**Priority**: MUST HAVE

**Requirements**:
- Firmware SHALL run on M5Stack Core2 (ESP32-D0WDQ6-V3)
- Firmware SHALL support Core2 v1.0 and v1.1 (AXP192 and AXP2101)
- Firmware SHALL work with optional HMI module (encoder + buttons)

**Measurement**: Hardware compatibility test

#### NFR-2.7.2 Library Versions
**Priority**: MUST HAVE

**Requirements**:
- M5Unified SHALL use latest stable release (check Context7)
- ArduinoJson SHALL use v7.x (not v6)
- FastLED SHALL support SK6812 RGBW LEDs

**Measurement**: Dependency version check

---

## 3. Hardware Specifications

### 3.1 M5Stack Core2

**Component** | **Specification**
---|---
CPU | ESP32-D0WDQ6-V3 (Dual-core Xtensa LX6 @ 240 MHz)
SRAM | 520 KB
PSRAM | 8 MB
Flash | 16 MB
Display | 320×240 TFT LCD (2.0")
Touch | FT6336U capacitive touch
IMU | MPU6886 (6-axis, I2C)
Audio | NS4168 I2S amplifier
LED Strip | 10× SK6812 RGBW (GPIO 25)
Power | AXP192 PMIC
Battery | 390 mAh LiPo (non-removable)
WiFi | 802.11 b/g/n (2.4 GHz)
Bluetooth | BLE 4.2 (not used in v2)

**Critical Constraints**:
- MPU6886 interrupt pin NOT connected to GPIO (polling only)
- Deep sleep cannot wake via gyro (only RTC timer or touch)

### 3.2 M5Stack HMI Module (Optional)

**Component** | **Specification**
---|---
Encoder | Rotary encoder (4 counts/detent)
Buttons | 2× physical buttons (A, B)
Interface | I2C @ 100 kHz (GPIO 21/22, Grove port)
Controller | STM32-based

---

## 4. Software Specifications

### 4.1 Development Environment

**Tool** | **Version** | **Purpose**
---|---|---
PlatformIO | Latest | Build system
VSCode | Latest | IDE
Git | 2.x | Version control
Python | 3.8+ | PlatformIO dependency

### 4.2 Libraries

**Library** | **Version** | **Purpose**
---|---|---
M5Unified | Latest stable (check Context7) | Hardware abstraction
ArduinoJson | 7.x | JSON parsing
PubSubClient | 2.8 | MQTT client
FastLED | 3.6+ | LED control
ESP32Time | Latest | Software RTC
LittleFS | Built-in | Filesystem

### 4.3 Toolchain

**Component** | **Version**
---|---
Arduino Framework | 2.x (ESP32 core)
ESP-IDF | 4.4+ (underlying)
GCC | 8.4+ (ARM)
FreeRTOS | 10.x (built-in)

---

## 5. Testing Requirements

### 5.1 Unit Tests

**Test Suite** | **Coverage Target** | **Test Count (Est.)**
---|---|---
State Machine | 80% | 15
Sequence Logic | 80% | 10
Statistics | 70% | 12
Time Manager | 60% | 8
**Total** | **70% avg** | **45+**

### 5.2 Integration Tests

**Test Scenario** | **Duration** | **Pass Criteria**
---|---|---
Complete 4-session sequence | 2 hours | All transitions correct
Pause/resume during session | 15 min | Time preserved
Gyro gestures (all orientations) | 30 min | 99% accuracy
Statistics (7-day accumulation) | 7 days | Counts match expected
WiFi disconnect/reconnect | 1 hour | No data loss
Battery drain (balanced mode) | 4+ hours | Measured runtime
Deep sleep + wake | 10 cycles | Timer resumes correctly

### 5.3 Acceptance Tests

**Feature** | **Test Method** | **Pass Criteria**
---|---|---
Timer accuracy | Compare to NTP | <1s drift/hour
Battery life | 100% → 0% test | ≥4h balanced mode (or document actual)
Cloud sync | Shadow state match | 99% reliability
UI responsiveness | Frame time measure | ≥10 FPS
Gyro detection | Orientation test | <1% false triggers

---

## 6. Documentation Requirements

### 6.1 Developer Documentation

**Document** | **Status** | **Owner**
---|---|---
Architecture diagram | Required | Dev team
State machine diagram | Required | Dev team
API documentation | Required | Dev team
Build instructions | Required | Dev team
ADRs (8 total) | Required | Dev team

### 6.2 User Documentation

**Document** | **Status** | **Owner**
---|---|---
Quick start guide | Required | Dev team
Gesture tutorial | Required | Dev team
Settings guide | Required | Dev team
Troubleshooting | Required | Dev team
Battery tips | Required | Dev team

---

## 7. Constraints and Assumptions

### 7.1 Constraints
1. Must use M5Unified library (M5Core2 deprecated)
2. MPU6886 interrupt not wired (polling only)
3. 520KB SRAM (FreeRTOS overhead ~100KB)
4. 390mAh battery (fixed capacity)
5. No backward compatibility with v1

### 7.2 Assumptions
1. AWS IoT account available
2. Existing Lambda functions work with v2
3. WiFi network available (2.4 GHz)
4. User has M5Stack Core2 hardware
5. NTP servers reachable from network

---

## 8. Acceptance Criteria Summary

**Phase 1: Local Timer** (Must-Have)
- ✅ Classic sequence completes correctly
- ✅ Gyro gestures work reliably (±60°)
- ✅ Statistics accurate over 7 days
- ✅ Settings persist across reboots
- ✅ Battery ≥3 hours (measured)
- ✅ UI renders 10 FPS
- ✅ No crashes in 24-hour test

**Phase 2: Cloud Integration** (Should-Have)
- ✅ Shadow syncs state correctly
- ✅ Toggl integration works
- ✅ Google Calendar logs sessions
- ✅ Offline queue prevents data loss
- ✅ Reconnect <2 seconds

**Phase 3: Polish** (Nice-to-Have)
- ✅ User docs complete
- ✅ 70% test coverage
- ✅ No files >300 lines
- ✅ README with build guide
- ✅ 7-day uptime validated

---

**Document Version**: 1.0
**Last Updated**: 2025-01-03
**Next Review**: After Phase 1 completion
**Status**: APPROVED
