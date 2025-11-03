# Changelog

All notable changes to M5 Pomodoro Timer will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Initial project structure
- Comprehensive planning documentation (10 documents, ~40,000 words)
- 8 Architecture Decision Records (ADRs)
- PlatformIO configuration with M5Unified library
- Custom partition table for OTA updates
- Basic main.cpp skeleton

### Changed
- Migrated from M5Core2 library to M5Unified (v0.1.13+)

### Technical Details
- Platform: M5Stack Core2 (ESP32-D0WDQ6-V3)
- Framework: Arduino (ESP-IDF underneath)
- Build System: PlatformIO
- Target: 4+ hour battery life in Balanced mode

## [1.0.0] - 2024-06-01

### Added
- Initial v1 release (legacy version, deprecated)
- Basic Pomodoro timer functionality
- AWS IoT integration
- Toggl sync

### Known Issues (v1)
- Monolithic code structure
- Race conditions in dual-core implementation
- No pause functionality
- High power consumption (~100mA)
- UI lag during network operations

---

## Version 2.0.0 Goals

### Core Features
- ✅ Classic Pomodoro sequence (25-5-25-5-25-5-25-15)
- ✅ Study mode (45-15)
- ✅ Gyro gesture controls (rotate to start, flat to resume)
- ✅ 90-day statistics with streak tracking
- ✅ Three power modes (Performance/Balanced/Battery Saver)
- ✅ AWS IoT Device Shadow sync
- ✅ Toggl and Google Calendar integration

### Architecture
- ✅ Dual-core FreeRTOS with explicit core pinning
- ✅ Proper synchronization (mutexes, queues, event groups)
- ✅ Light sleep during timer (94% power reduction)
- ✅ NVS-based statistics storage (OTA-safe)
- ✅ Gyro polling (INT pin not connected on Core2)

### Documentation
- ✅ 10 comprehensive planning documents
- ✅ 8 Architecture Decision Records
- ✅ Project-specific development guide (CLAUDE.md)
- ✅ YouTrack task tracking (42 tasks with dependencies)

### Phase 1: Local Timer (In Progress)
- [ ] Project setup and configuration
- [ ] Core timer state machine
- [ ] Gyro gesture detection
- [ ] UI rendering (5 screens, 6 widgets)
- [ ] Statistics tracking
- [ ] Power management
- [ ] Audio and LED feedback

### Phase 2: Cloud Integration (Planned)
- [ ] AWS IoT Device Shadow sync
- [ ] Toggl time tracking
- [ ] Google Calendar events
- [ ] NTP time synchronization
- [ ] Offline queue

### Phase 3: Testing & Polish (Planned)
- [ ] Unit tests (70%+ coverage)
- [ ] Integration tests
- [ ] Battery life testing
- [ ] UI/UX refinement
- [ ] Performance optimization
