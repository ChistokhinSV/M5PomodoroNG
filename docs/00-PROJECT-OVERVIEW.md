# M5 Pomodoro v2 - Project Overview

## Executive Summary

M5 Pomodoro v2 is a complete rebuild of the M5Stack-based Pomodoro timer with enhanced architecture, battery optimization, and gyroscope-based gesture control. This project creates a portable productivity device that combines autonomous local operation with cloud synchronization.

**Version**: 2.0.0
**Platform**: M5Stack Core2 (ESP32 + 390mAh battery)
**Development Status**: Planning Phase
**Target Completion**: 8 weeks (~120 hours)

---

## Key Features

### Core Timer Functionality
- **Classic Pomodoro Sequence**: 25-5-25-5-25-5-25-15 (4 work sessions with short/long breaks)
- **Study Mode**: 45-15-45-15... (extended work/rest cycle)
- **Custom Durations**: User-configurable work (5-90 min) and rest (1-30 min) periods
- **Pause/Resume**: Preserve exact time remaining when paused
- **Auto-start Options**: Configurable automatic transitions

### Gesture Control (NEW)
- **Rotation Detection**: Rotate device ±60° to start/pause timer
- **Flat Detection**: Lay device flat (screen up) to start next session
- **Emergency Stop**: Double-shake to stop timer immediately
- **Configurable**: Enable/disable, adjust sensitivity (30-90°)

### Statistics Tracking (NEW)
- **Daily Counts**: Track completed pomodoros per day
- **90-Day History**: Circular buffer in NVS storage
- **Streak Tracking**: Consecutive days with ≥1 pomodoro
- **Weekly Chart**: Visual bar chart (Mon-Sun) on dedicated screen
- **Completion Rate**: Finished vs started ratio
- **Lifetime Total**: All-time pomodoro counter

### Battery Optimization (PRIMARY FOCUS)
- **Three Power Modes**: Performance, Balanced (default), Battery Saver
- **Adaptive Behavior**: Auto-switch based on battery level
- **Smart Sleep**: Light sleep during timer, deep sleep on timeout
- **WiFi Management**: Always-on / periodic / on-demand per mode
- **Target Runtime**: 4+ hours in balanced mode (measure and document actual)

### Enhanced UI/UX
- **Modern Design**: High-contrast colors, large fonts, smooth animations
- **Sequence Indicator**: Visual progress (●●○○) for classic mode
- **Multi-Modal Feedback**: Audio alerts, LED animations, haptic vibration
- **Settings Screen**: Comprehensive configuration interface
- **Statistics Screen**: Weekly progress visualization

### Cloud Integration
- **AWS IoT Device Shadow**: State synchronization across devices
- **Toggl Webhook**: Automatic time tracking integration
- **Google Calendar**: Session logging
- **Offline-First**: Full functionality without network
- **Optimized Sync**: Minimal bandwidth, adaptive intervals

---

## Goals

### Primary Goals
1. **Battery Life**: Achieve ≥4 hours runtime in balanced mode (or document actual + recommend charger)
2. **Gesture Control**: Reliable rotation detection without false triggers
3. **Statistics System**: Accurate tracking with 90-day retention
4. **Code Quality**: Modular architecture, no files >300 lines
5. **User Experience**: Responsive UI (10 FPS), instant feedback

### Secondary Goals
6. **Test Coverage**: 70% unit test coverage for core logic
7. **Documentation**: Comprehensive planning docs + user guides
8. **Performance**: <500ms gesture detection, <2s MQTT reconnect
9. **Memory Efficiency**: <200KB SRAM, <2MB PSRAM, <12MB Flash
10. **Reliability**: 7-day continuous operation without crashes

---

## Non-Goals

### Explicitly Out of Scope
- **Wake from deep sleep via gyro**: Hardware limitation (MPU6886 interrupt not connected to GPIO)
- **Backward compatibility**: No v1 device support (breaking changes allowed)
- **Multi-device config sync**: Single device focus
- **Voice commands**: Not implementing speech recognition
- **Alternative displays**: Targeting Core2's 320×240 LCD only
- **Additional integrations**: Beyond Toggl + Google Calendar (for now)
- **Custom LED patterns**: Preset animations only

---

## Success Metrics

### Technical Metrics
- ✅ **Rendering**: 10 FPS (100ms frame time) with dirty rectangles
- ✅ **Battery**: 4+ hours in balanced mode (or document actual)
- ✅ **Reconnect**: <2s MQTT reconnection with SSL session resume
- ✅ **Gesture Latency**: <500ms from rotation to timer action
- ✅ **Memory**: SRAM <200KB, PSRAM <2MB, Flash <12MB
- ✅ **Time Accuracy**: <1s drift over 8 hours with NTP sync

### Functional Metrics
- ✅ **Sequence Tracking**: Complete 4-session classic cycle correctly
- ✅ **Statistics**: Accurate counts, streaks, and completion rates
- ✅ **Persistence**: Settings and stats survive reboots
- ✅ **Cloud Sync**: 99% reliable state synchronization
- ✅ **Gesture Detection**: Works in all orientations with <1% false triggers

### Quality Metrics
- ✅ **Code Organization**: No files >300 lines, modular structure
- ✅ **Test Coverage**: 70% for state machine, sequence, statistics
- ✅ **Build Time**: <60 seconds full build
- ✅ **Stability**: 7-day operation without crashes or memory leaks
- ✅ **Documentation**: All ADRs + 10 planning docs + user guide

---

## Constraints

### Hardware Constraints
- **SRAM**: 520KB (carefully managed, FreeRTOS overhead)
- **PSRAM**: 8MB (used for canvas buffer, sprite cache)
- **Flash**: 16MB (firmware + assets + filesystem)
- **Battery**: 390mAh (non-linear discharge curve)
- **Display**: 320×240 TFT (fixed resolution)
- **Gyro Interrupt**: NOT connected to GPIO (polling only, no wake from deep sleep)

### Platform Constraints
- **Dual-Core ESP32**: Core 0 (UI), Core 1 (network)
- **AXP192 Power**: Deep sleep ~0.26mA, but gyro cannot wake
- **WiFi Power**: ~80mA active, manage duty cycle aggressively
- **M5Unified Library**: Must use (M5Core2 deprecated)

### Time Constraints
- **8 Weeks Target**: Part-time development (~15h/week = 120h total)
- **Phase 1-2**: Documentation + local timer (4 weeks)
- **Phase 3**: Cloud integration (2 weeks)
- **Phase 4**: Testing + polish (2 weeks)

### Technical Constraints
- **FreeRTOS**: Required for dual-core synchronization
- **I2C Bus**: Shared by HMI + MPU6886, must serialize access
- **NVS Storage**: ~15KB available for config + statistics
- **MQTT Buffer**: 8KB limit for shadow documents

---

## Technology Stack

### Core Platform
- **Hardware**: M5Stack Core2 (ESP32-D0WDQ6-V3)
- **Framework**: Arduino + PlatformIO
- **Library**: M5Unified (latest stable, check Context7)
- **RTOS**: FreeRTOS (built-in ESP32)

### Key Libraries
- **M5Unified**: Hardware abstraction (display, touch, IMU, power)
- **ArduinoJson**: JSON parsing for MQTT shadow documents
- **PubSubClient**: MQTT client for AWS IoT Core
- **FastLED**: SK6812 LED strip control
- **ESP32Time**: Software RTC with NTP sync
- **LittleFS**: Filesystem for assets (WAV files, PNGs)

### Cloud Services
- **AWS IoT Core**: Device Shadow for state sync
- **AWS Lambda**: Webhook processing (Toggl, Google Calendar)
- **Amazon SNS/SQS**: Event processing pipeline
- **NTP**: pool.ntp.org, time.google.com, time.cloudflare.com

### Development Tools
- **PlatformIO**: Build system + dependency management
- **Git**: Version control
- **YouTrack**: Issue tracking (project MP)
- **VSCode**: IDE with PlatformIO extension

---

## Architecture Overview

### Modular Structure
```
src/
├── core/          # Business logic (timer, sequence, stats, config)
├── hardware/      # Hardware abstraction (LED, audio, gyro, power)
├── ui/            # User interface (screens, widgets, renderer)
├── network/       # Cloud connectivity (WiFi, MQTT, shadow sync)
└── main.cpp       # Initialization only (<100 lines)
```

### FreeRTOS Tasks
- **Core 0 (main loop)**: UI rendering, touch/gyro input, timer state machine
- **Core 1 (networkTask)**: WiFi, MQTT, NTP synchronization
- **Synchronization**: Queues for inter-core messages, mutexes for shared state

### Data Flow
1. **User Input** → Gyro/Touch → State Machine → Update UI
2. **Timer Tick** → State Machine → Statistics → UI + Feedback (audio/LED)
3. **State Change** → Queue → Network Task → MQTT Publish
4. **Cloud Command** → MQTT Subscribe → Queue → State Machine → UI

---

## Risk Assessment

### High-Risk Items
1. **Battery Life Goal**: 4 hours may be optimistic
   - **Mitigation**: Aggressive WiFi duty cycling, early testing, document actual if lower
2. **Gyro False Triggers**: Accidental starts from normal movement
   - **Mitigation**: Strict 60° threshold, 500ms debounce, configurable sensitivity
3. **Timeline**: 8 weeks ambitious for feature scope
   - **Mitigation**: Prioritize local timer first, defer cloud if needed

### Medium-Risk Items
4. **Statistics Storage**: 90 days × 50 bytes = 4.5KB (NVS limits)
   - **Mitigation**: NVS has 15KB partition, sufficient headroom
5. **Render Performance**: 10 FPS may not achieve with full UI
   - **Mitigation**: Sprite caching, dirty rectangles, profile early
6. **I2C Conflicts**: HMI + MPU6886 on same bus
   - **Mitigation**: FreeRTOS mutex, restrict to Core 0 only

### Low-Risk Items
7. **NTP Reliability**: Public pools occasionally slow
   - **Mitigation**: Multiple servers, exponential backoff
8. **SSL Session Size**: 8KB MQTT buffer sufficient
   - **Mitigation**: Optimized shadow schema, tested in v1

---

## Dependencies

### External Dependencies
- **AWS IoT Core**: Account + device certificate (existing)
- **Toggl API**: Webhook integration (existing Lambda)
- **Google Calendar API**: OAuth + service account (existing Lambda)
- **NTP Servers**: Internet access for time sync

### Internal Dependencies (Task Order)
1. **Documentation** (MP-2, MP-3) → **Project Setup** (MP-4)
2. **Project Setup** → **Hardware Abstraction** (MP-7, MP-8, MP-9)
3. **Hardware** → **State Machine** (MP-10, MP-11)
4. **State Machine** → **UI Screens** (MP-17, MP-18, MP-19)
5. **UI + State** → **Local Testing** (MP-30)
6. **Local Working** → **Cloud Integration** (MP-34, MP-35, MP-36)
7. **Cloud** → **Final Testing** (MP-40)

---

## Stakeholders

### Primary Users
- **Individual**: Personal productivity, Pomodoro practitioners
- **Developer (self)**: Testing, debugging, feature development

### Secondary Users
- **Future Users**: GitHub community (if open-sourced)

---

## Project Timeline

### Phase 1: Documentation & Planning (Week 1)
- Write 10 planning documents
- Create 8 ADRs
- Setup YouTrack with 42 tasks
- **Deliverable**: Complete planning documentation

### Phase 2: Local Timer (Week 2-7)
- Core infrastructure, state machine, UI, statistics
- Hardware abstraction, gyro control, power management
- Integration testing
- **Deliverable**: Fully functional local timer

### Phase 3: Cloud Integration (Week 7-8)
- WiFi + MQTT + AWS IoT shadow sync
- Adaptive sync strategy
- Cloud testing
- **Deliverable**: Cloud-connected timer

### Phase 4: Polish & Documentation (Week 8)
- Final testing (7-day uptime, battery validation)
- User documentation
- Code cleanup
- **Deliverable**: Production-ready firmware

---

## Key Decisions

### Architectural Decisions (ADRs)
1. **M5Unified over M5Core2**: Current library, active development
2. **Dual-Core Architecture**: Core 0 UI, Core 1 network (proven in v1)
3. **FreeRTOS Synchronization**: Queues/mutexes instead of volatile
4. **Gyro Polling**: Hardware constraint (interrupt not wired)
5. **Light Sleep During Timer**: Keep peripherals active for gyro
6. **NVS Statistics**: 90-day circular buffer, efficient storage
7. **Classic Sequence Tracking**: Primary mode, 4-session logic
8. **Three-Tier Power Modes**: Performance, Balanced, Battery Saver

---

## Success Criteria

### Must-Have (Phase 2 - Local Timer)
- ✅ Classic Pomodoro sequence works correctly
- ✅ Gyro gestures detect rotation reliably
- ✅ Statistics track daily counts accurately
- ✅ Settings persist across reboots
- ✅ Battery lasts ≥3 hours (measured)
- ✅ UI renders at 10 FPS
- ✅ No crashes in 24-hour test

### Should-Have (Phase 3 - Cloud)
- ✅ AWS IoT shadow syncs state
- ✅ Toggl integration logs time entries
- ✅ Google Calendar creates events
- ✅ Offline queue works correctly
- ✅ Reconnect <2 seconds with SSL resume

### Nice-to-Have (Phase 4 - Polish)
- ✅ User documentation complete
- ✅ 70% test coverage achieved
- ✅ Code cleanup (no files >300 lines)
- ✅ README with build instructions
- ✅ 7-day uptime validated

---

## References

### Related Documents
- [01-TECHNICAL-REQUIREMENTS.md](01-TECHNICAL-REQUIREMENTS.md) - Detailed requirements
- [02-ARCHITECTURE.md](02-ARCHITECTURE.md) - System architecture
- [03-STATE-MACHINE.md](03-STATE-MACHINE.md) - Timer state logic
- [06-POWER-MANAGEMENT.md](06-POWER-MANAGEMENT.md) - Battery optimization
- [09-BUILD-DEPLOY.md](09-BUILD-DEPLOY.md) - Build instructions

### External Resources
- M5Stack Core2 Docs: https://docs.m5stack.com/en/core/core2
- M5Unified GitHub: https://github.com/m5stack/M5Unified
- AWS IoT Device Shadow: https://docs.aws.amazon.com/iot/latest/developerguide/iot-device-shadows.html
- Pomodoro Technique: https://francescocirillo.com/pages/pomodoro-technique

---

**Document Version**: 1.0
**Last Updated**: 2025-01-03
**Author**: Claude (AI Assistant)
**Status**: Planning Complete
