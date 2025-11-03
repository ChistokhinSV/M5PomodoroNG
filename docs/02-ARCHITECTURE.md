# System Architecture - M5 Pomodoro v2

## Document Information
- **Version**: 1.0
- **Date**: 2025-01-03
- **Status**: Draft
- **Related**: [00-PROJECT-OVERVIEW.md](00-PROJECT-OVERVIEW.md), [01-TECHNICAL-REQUIREMENTS.md](01-TECHNICAL-REQUIREMENTS.md)

---

## 1. Architecture Overview

### 1.1 High-Level Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        M5Stack Core2                         │
│  ┌────────────────────────────────────────────────────────┐ │
│  │              ESP32 Dual-Core (240 MHz)                  │ │
│  │                                                          │ │
│  │  ┌──────────────────┐      ┌──────────────────┐       │ │
│  │  │   Core 0 (PRO)   │      │   Core 1 (APP)   │       │ │
│  │  │                  │      │                  │       │ │
│  │  │  UI Rendering    │      │  Network Task    │       │ │
│  │  │  Touch Input     │      │  - WiFi          │       │ │
│  │  │  Gyro Polling    │      │  - MQTT          │       │ │
│  │  │  State Machine   │      │  - NTP Sync      │       │ │
│  │  │  LED/Audio       │      │                  │       │ │
│  │  │                  │◄────►│  FreeRTOS Queues │       │ │
│  │  └──────────────────┘      └──────────────────┘       │ │
│  └────────────────────────────────────────────────────────┘ │
│                                                              │
│  Hardware: Display, Touch, MPU6886, AXP192, LEDs, Audio    │
└─────────────────────────────────────────────────────────────┘
                          │
                          │ WiFi (2.4 GHz)
                          ▼
             ┌────────────────────────┐
             │      AWS IoT Core      │
             │  (Device Shadow MQTT)  │
             └────────────────────────┘
                          │
                          ▼
             ┌────────────────────────┐
             │    Lambda Functions    │
             │  - Toggl Webhook       │
             │  - Google Calendar     │
             └────────────────────────┘
```

### 1.2 Architectural Principles

1. **Dual-Core Separation**: UI on Core 0, Network on Core 1
2. **Offline-First**: Full functionality without network
3. **Modular Design**: 11 independent components with clear interfaces
4. **Event-Driven**: State machine emits events, components react
5. **Hardware Abstraction**: Isolate M5Unified dependencies for testability
6. **Battery-Aware**: Three power modes with adaptive behavior

---

## 2. Component Architecture

### 2.1 Component Diagram

```
┌──────────────────────────────────────────────────────────────┐
│                         Core 0 Loop                           │
│                                                               │
│  ┌─────────────┐      ┌──────────────────────────────────┐ │
│  │   main.cpp  │──────►  ScreenManager                    │ │
│  │  (init only)│      │  - Route screens                  │ │
│  └─────────────┘      │  - Handle navigation              │ │
│                       └──────────────────────────────────┘ │
│                                  │                          │
│                       ┌──────────┴──────────┐              │
│                       ▼                     ▼              │
│              ┌──────────────┐      ┌──────────────┐       │
│              │   Screens    │      │   Widgets    │       │
│              │ - Main       │      │ - ProgressBar│       │
│              │ - Stats      │      │ - StatusBar  │       │
│              │ - Settings   │      │ - Chart      │       │
│              │ - Pause      │      │ - Indicator  │       │
│              └──────────────┘      └──────────────┘       │
│                       │                                     │
│                       ▼                                     │
│              ┌──────────────┐                              │
│              │   Renderer   │                              │
│              │ - Canvas     │                              │
│              │ - Dirty rect │                              │
│              │ - Sprites    │                              │
│              └──────────────┘                              │
│                                                             │
│  ┌────────────────────────────────────────────────────┐   │
│  │              Core Business Logic                    │   │
│  │  ┌──────────────────┐   ┌──────────────────┐      │   │
│  │  │ TimerStateMachine│◄──┤ PomodoroSequence │      │   │
│  │  │ - 5 states       │   │ - Classic/Study  │      │   │
│  │  │ - 6 events       │   │ - Session track  │      │   │
│  │  └──────────────────┘   └──────────────────┘      │   │
│  │           │                       │                │   │
│  │           ├───────────────────────┼────────┐       │   │
│  │           ▼                       ▼        ▼       │   │
│  │  ┌──────────────┐   ┌──────────────┐  ┌────────┐ │   │
│  │  │  Statistics  │   │ TimeManager  │  │ Config │ │   │
│  │  │ - 90-day NVS │   │ - NTP sync   │  │ - NVS  │ │   │
│  │  └──────────────┘   └──────────────┘  └────────┘ │   │
│  └────────────────────────────────────────────────────┘   │
│                                                             │
│  ┌────────────────────────────────────────────────────┐   │
│  │           Hardware Abstraction Layer                │   │
│  │  ┌───────────┐ ┌──────────┐ ┌──────────────┐      │   │
│  │  │GyroCtrl   │ │LEDCtrl   │ │AudioPlayer   │      │   │
│  │  └───────────┘ └──────────┘ └──────────────┘      │   │
│  │  ┌───────────┐ ┌──────────┐                       │   │
│  │  │PowerMgr   │ │HMICtrl   │                       │   │
│  │  └───────────┘ └──────────┘                       │   │
│  └────────────────────────────────────────────────────┘   │
│                       │                                     │
│                       ▼                                     │
│              ┌──────────────┐                              │
│              │  M5Unified   │                              │
│              │ M5.Display   │                              │
│              │ M5.Touch     │                              │
│              │ M5.Imu       │                              │
│              │ M5.Power     │                              │
│              └──────────────┘                              │
└──────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────┐
│                       Core 1 Network Task                     │
│                                                               │
│  ┌────────────────────────────────────────────────────┐     │
│  │  WiFiManager   ◄───┐                               │     │
│  │  - Connect     │   │ Power mode aware              │     │
│  │  - Reconnect   │   │                               │     │
│  └────────┬───────┘   │                               │     │
│           │           │                               │     │
│           ▼           │                               │     │
│  ┌────────────────┐   │                               │     │
│  │  MQTTClient    │   │                               │     │
│  │  - TLS 1.2     │   │                               │     │
│  │  - 8KB buffer  │   │                               │     │
│  └────────┬───────┘   │                               │     │
│           │           │                               │     │
│           ▼           │                               │     │
│  ┌────────────────┐   │                               │     │
│  │  ShadowSync    │───┘                               │     │
│  │  - Publish     │                                   │     │
│  │  - Subscribe   │                                   │     │
│  │  - Queue       │                                   │     │
│  └────────────────┘                                   │     │
│           │                                            │     │
│           │ FreeRTOS Queue                            │     │
│           ▼                                            │     │
│      [Core 0] ◄───── State updates                    │     │
└──────────────────────────────────────────────────────────────┘
```

### 2.2 Module Breakdown

#### Core Modules (`src/core/`)
- **PomodoroSequence**: Classic (4-session) and study mode logic
- **TimerStateMachine**: State transitions, event handling
- **Statistics**: 90-day circular buffer, NVS persistence
- **Config**: User settings, NVS storage
- **TimeManager**: NTP sync, RTC, drift compensation

#### Hardware Modules (`src/hardware/`)
- **GyroController**: MPU6886 polling, rotation detection
- **LEDController**: FastLED SK6812 animations
- **AudioPlayer**: I2S WAV playback
- **PowerManager**: AXP192 battery, sleep modes
- **HMIController**: Optional encoder + buttons

#### UI Modules (`src/ui/`)
- **ScreenManager**: Screen routing, navigation stack
- **Renderer**: Canvas, dirty rectangles, sprite cache
- **Screens**: MainScreen, StatsScreen, SettingsScreen, PauseScreen
- **Widgets**: ProgressBar, StatusBar, SequenceIndicator, StatsChart

#### Network Modules (`src/network/`)
- **WiFiManager**: Connection, power-aware WiFi
- **MQTTClient**: AWS IoT MQTT over TLS
- **ShadowSync**: Device Shadow protocol

---

## 3. Data Flow Architecture

### 3.1 Timer Start Flow

```
User Action (touch/gyro)
    → Input Handler (Core 0)
    → TimerStateMachine.handleEvent(START)
    → Update PomodoroSequence (increment session if needed)
    → Emit STATE_CHANGED event
    ├─→ UI: Update main screen (timer display)
    ├─→ Hardware: Start LED animation (green breathe)
    ├─→ Hardware: Play start.wav audio
    └─→ Network Queue: Publish state to shadow (Core 1)
```

### 3.2 Timer Expiration Flow

```
Ticker (1s interval)
    → TimerStateMachine.tick()
    → Remaining time = 0?
    │
    ├─ Yes → handleEvent(EXPIRE)
    │         ├─→ Statistics.incrementCompleted()
    │         ├─→ PomodoroSequence.advanceSession()
    │         ├─→ UI: Transition to rest screen
    │         ├─→ Hardware: Play rest.wav, LED blue fade
    │         └─→ Network Queue: Publish completion
    │
    └─ No → Continue countdown
```

### 3.3 Cloud Sync Flow (Core 0 → Core 1)

```
[Core 0] Timer state change
    → ShadowSync.queueUpdate(state)
    → FreeRTOS Queue: Send to Core 1

[Core 1] Network task loop
    → xQueueReceive(shadowQueue, &update)
    → WiFi connected?
    │
    ├─ Yes → MQTTClient.publish("shadow/update", json)
    │         └─→ AWS IoT receives update
    │
    └─ No → Store in offline queue (max 10 updates)
            └─→ Will retry on reconnect
```

### 3.4 Cloud Command Flow (Core 1 → Core 0)

```
[Core 1] MQTT callback
    → MQTTClient.onMessage("shadow/update/accepted")
    → Parse JSON: desired state
    → ShadowSync.handleDesiredState()
    → FreeRTOS Queue: Send to Core 0

[Core 0] Main loop
    → xQueueReceive(commandQueue, &command)
    → TimerStateMachine.applyCloudCommand()
    → UI updates reflect new state
```

### 3.5 Statistics Collection Flow

```
Timer EXPIRE event
    → Statistics.onPomodoroComplete()
    → Load daily stats from NVS (key: stats_YYYYMMDD)
    → Increment pomodoros_completed
    → Update total_work_minutes
    → Recalculate current_streak_days
    → Save to NVS (atomic write)

Midnight rollover (00:00 UTC)
    → TimeManager detects date change
    → Statistics.onDayChange()
    → Create new day entry
    → Update 90-day circular buffer (delete oldest if >90)
```

---

## 4. FreeRTOS Task Architecture

### 4.1 Task Configuration

| Task | Core | Priority | Stack | Function |
|------|------|----------|-------|----------|
| Main Loop | 0 | 1 (default) | 8KB | UI, input, state machine |
| Network Task | 1 | 1 | 10KB | WiFi, MQTT, NTP |
| updateControls Ticker | 0 | - | - | Poll gyro/encoder (10ms) |
| renderTicker | 0 | - | - | Render UI (100ms) |
| timerTicker | 0 | - | - | Timer countdown (1s) |

### 4.2 Inter-Core Communication

**Queues** (Core 1 → Core 0):
```cpp
QueueHandle_t uiUpdateQueue;       // Shadow updates to UI
QueueHandle_t networkEventQueue;   // WiFi/MQTT status
```

**Queues** (Core 0 → Core 1):
```cpp
QueueHandle_t shadowPublishQueue;  // State changes to publish
```

**Mutexes** (Shared Resources):
```cpp
SemaphoreHandle_t i2cBusMutex;     // HMI + MPU6886 access
SemaphoreHandle_t shadowStateMutex; // Protect shadow state struct
SemaphoreHandle_t statsMutex;       // Statistics write protection
```

**Event Groups** (Status Flags):
```cpp
EventGroupHandle_t networkEvents;
// Bits: WIFI_CONNECTED (0), MQTT_CONNECTED (1), NTP_SYNCED (2)
```

### 4.3 Task Synchronization Pattern

```cpp
// Core 0: Publish state change
void publishStateChange(PomodoroState state) {
    ShadowUpdate update = {state, rtc.getEpoch(), duration};
    xQueueSend(shadowPublishQueue, &update, 0); // Non-blocking
}

// Core 1: Process publish queue
void networkTask(void* params) {
    while(1) {
        ShadowUpdate update;
        if (xQueueReceive(shadowPublishQueue, &update, portMAX_DELAY)) {
            if (wifi_connected && mqtt_connected) {
                shadowSync.publish(update);
            } else {
                offlineQueue.push(update);
            }
        }
    }
}
```

---

## 5. Memory Architecture

### 5.1 Memory Map

| Memory Type | Size | Usage | Components |
|-------------|------|-------|------------|
| **SRAM** | 520KB | 200KB target | Stack, heap, globals |
| **PSRAM** | 8MB | 2MB target | Canvas buffer, sprite cache |
| **Flash** | 16MB | 12MB target | Firmware, assets, filesystem |
| **NVS** | 15KB | 10KB target | Config, statistics |

### 5.2 SRAM Allocation (Target: <200KB)

```
Stack (Core 0 main):       8KB
Stack (Core 1 network):   10KB
FreeRTOS kernel:          30KB
Heap (dynamic):           80KB
Global variables:         20KB
M5Unified buffers:        40KB
MQTT buffers:              8KB
Shadow state:              2KB
Misc:                      2KB
--------------------------------
Total:                   ~200KB
Reserve:                 320KB (buffer for fragmentation)
```

### 5.3 PSRAM Allocation (Target: <2MB)

```
Canvas back buffer:      150KB (320×240×2 bytes)
Sprite cache (10):       500KB (icons, backgrounds)
Font glyph cache:         50KB (digits 0-9, :, letters)
Temporary buffers:       100KB
Audio WAV buffers:       200KB (preloaded sounds)
--------------------------------
Total:                  ~1000KB
Reserve:                 7MB (plenty of headroom)
```

### 5.4 Flash Allocation (Target: <12MB)

```
Firmware binary:          2MB
LittleFS partition:       5MB
  - Audio files:        1MB (start.wav, rest.wav, etc.)
  - Images:             500KB (backgrounds, icons PNGs)
  - Fonts:              500KB (Orbitron TTF)
OTA partition:            5MB (for future OTA updates)
Bootloader/partitions:    1MB
--------------------------------
Total:                   ~13MB (fits in 16MB)
```

### 5.5 NVS Storage (Target: <10KB)

```
Config namespace:        1KB
  - UserConfig struct:  ~200 bytes

Statistics namespace:    9KB
  - Daily stats (90):   90 × 50 bytes = 4.5KB
  - Lifetime counter:   8 bytes
  - Streak data:        50 bytes
  - Metadata:           ~500 bytes
--------------------------------
Total:                  ~10KB (fits in 15KB partition)
```

---

## 6. State Management Architecture

### 6.1 State Machine Design

**States**:
```cpp
enum class PomodoroState {
    STOPPED,      // Idle, ready to start
    POMODORO,     // 25min work session
    SHORT_REST,   // 5min break
    LONG_REST,    // 15min break
    PAUSED        // Timer paused
};
```

**Events**:
```cpp
enum class TimerEvent {
    START,        // Start new session
    PAUSE,        // Pause running timer
    RESUME,       // Resume from pause
    STOP,         // Stop timer, reset
    EXPIRE,       // Timer reached 00:00
    GYRO_ROTATE   // Gyro gesture detected
};
```

**Transition Table**:
```
Current State    | Event       | Next State    | Action
-----------------|-------------|---------------|------------------
STOPPED          | START       | POMODORO      | Start work timer
POMODORO         | PAUSE       | PAUSED        | Save remaining time
POMODORO         | STOP        | STOPPED       | Reset
POMODORO         | EXPIRE      | SHORT_REST/   | Advance session
                 |             | LONG_REST     | Play audio
SHORT_REST       | EXPIRE      | STOPPED       | Ready for next
LONG_REST        | EXPIRE      | STOPPED       | Sequence complete
PAUSED           | RESUME      | POMODORO      | Resume from pause
PAUSED           | STOP        | STOPPED       | Reset
ANY              | GYRO_ROTATE | Toggle        | Context-dependent
```

### 6.2 Sequence State

```cpp
struct PomodoroSequence {
    PomodoroMode mode;           // CLASSIC, STUDY, CUSTOM
    uint8_t current_session;     // 1-4 for CLASSIC (0 for STUDY)
    uint8_t completed_today;     // Daily counter
    uint32_t sequence_start;     // Epoch timestamp
};
```

**Classic Sequence Transitions**:
```
Session 1 (25min) → Short Rest (5min)
  → Session 2 (25min) → Short Rest (5min)
  → Session 3 (25min) → Short Rest (5min)
  → Session 4 (25min) → Long Rest (15min)
  → [Sequence Complete, reset to Session 1]
```

### 6.3 Persistent State

**Stored in NVS**:
- UserConfig (settings)
- Statistics (90-day history)

**Stored in RTC Memory** (survives deep sleep):
- Current timer state
- Start timestamp
- Remaining seconds (if paused)

**Stored in RAM** (volatile):
- UI state (current screen)
- Network connection status
- Temporary buffers

---

## 7. Power Management Architecture

### 7.1 Power Modes

| Mode | Screen | Gyro | WiFi | Render | Current Draw |
|------|--------|------|------|--------|--------------|
| PERFORMANCE | 100% | 100Hz | Always | 10 FPS | ~150mA |
| BALANCED | 80% | 50Hz | Periodic | 10 FPS | ~100mA |
| BATTERY_SAVER | 50% | 25Hz | On-demand | 4 FPS | ~60mA |
| Light Sleep | Off | Idle | Off | N/A | ~10mA |
| Deep Sleep | Off | Off | Off | N/A | <1mA |

### 7.2 Power State Transitions

```
Normal Operation (BALANCED)
    │
    ├─ Battery <20% ──→ BATTERY_SAVER (auto)
    │                    │
    │                    └─ Battery >30% ──→ BALANCED (auto)
    │
    ├─ User selection ──→ PERFORMANCE (manual)
    │
    ├─ Inactivity 300s ──→ Light Sleep
    │                    │
    │                    └─ Touch/Timer ──→ BALANCED (wake)
    │
    └─ Deep sleep trigger ──→ Deep Sleep
                         │
                         └─ Touch/RTC ──→ BALANCED (wake)
```

### 7.3 WiFi Duty Cycling

**PERFORMANCE Mode**:
```
WiFi: Always On
MQTT: Persistent connection, 60s keep-alive
Sync: Report every 60s
```

**BALANCED Mode** (default):
```
WiFi: Connect → Sync → Disconnect
Schedule: Every 120s
MQTT: Connect when syncing, disconnect after
Power savings: ~30% vs always-on
```

**BATTERY_SAVER Mode**:
```
WiFi: On-demand only (state change triggers)
MQTT: One-shot publish, immediate disconnect
Sync: On START, PAUSE, STOP, EXPIRE only
Power savings: ~60% vs always-on
```

---

## 8. Rendering Architecture

### 8.1 Rendering Pipeline

```
State Change
    │
    ▼
Screen Manager
    │
    ├─ Determine active screen
    ├─ Call screen.update()
    │   │
    │   └─ Mark dirty regions
    │
    ▼
Renderer (100ms ticker)
    │
    ├─ Check dirty rectangles
    │   │
    │   ├─ None → Skip render (save power)
    │   │
    │   └─ Dirty → Render
    │       │
    │       ├─ Draw to back buffer (M5Canvas in PSRAM)
    │       ├─ Composite sprites
    │       ├─ Draw text with cached glyphs
    │       └─ pushSprite() to display (DMA transfer)
    │
    └─ Clear dirty flags
```

### 8.2 Dirty Rectangle System

```cpp
struct DirtyRect {
    uint16_t x, y, w, h;
    bool dirty;
};

// Track 10 regions
DirtyRect dirtyRegions[10] = {
    {0, 0, 320, 24, true},      // Status bar
    {0, 24, 320, 30, false},    // Sequence indicator
    {80, 60, 160, 80, false},   // Timer display
    {0, 140, 320, 40, false},   // Progress bar
    // ... more regions
};
```

**Benefits**:
- Skip rendering unchanged regions
- Reduce PSRAM writes
- Improve frame time (target: <100ms)

### 8.3 Sprite Caching

```
Cache slot 0:  Background (320×240)
Cache slot 1:  WiFi connected icon (32×32)
Cache slot 2:  WiFi disconnected icon (32×32)
Cache slot 3:  MQTT connected icon (32×32)
Cache slot 4:  Battery icons ×5 (32×32 each)
Cache slot 5:  Tomato icon (64×64)
Cache slot 6-9: Reserved
```

**Cache Management**:
- Pre-load at startup from LittleFS
- Store in PSRAM (never reload from Flash)
- Composite onto back buffer during render

---

## 9. Network Architecture

### 9.1 MQTT Topic Structure

**Subscribe Topics**:
```
$aws/things/{THINGNAME}/shadow/get/accepted
$aws/things/{THINGNAME}/shadow/update/accepted
$aws/things/{THINGNAME}/shadow/update/rejected
```

**Publish Topics**:
```
$aws/things/{THINGNAME}/shadow/get
$aws/things/{THINGNAME}/shadow/update
```

### 9.2 Shadow Document Schema

**Optimized Schema (v2)**:
```json
{
  "state": {
    "reported": {
      "s": "POMODORO",       // state (shortened key)
      "t": 1735934000,       // start timestamp (UTC epoch)
      "d": 1500,             // duration seconds
      "n": "Update docs",    // task name/description
      "m": 0,                // mode (0=classic, 1=study, 2=custom)
      "seq": 2,              // session number (1-4, 0 for study)
      "v": "2.0.0"           // firmware version
    }
  }
}
```

**Size**: ~150 bytes (fits in 8KB MQTT buffer with headroom)

### 9.3 Offline Queue

```cpp
struct ShadowUpdate {
    PomodoroState state;
    uint32_t timestamp;
    uint16_t duration;
    char task_name[64];
};

std::deque<ShadowUpdate> offlineQueue; // Max 10 updates
```

**Queue Behavior**:
- Push on state change when offline
- Max 10 updates (FIFO, drop oldest if full)
- Publish all on reconnect (batch)
- Clear queue after successful publish

---

## 10. Security Architecture

### 10.1 TLS Configuration

```cpp
WiFiClientSecure net;
net.setCACert(AWS_CERT_CA);      // Amazon Root CA 1
net.setCertificate(AWS_CERT_CRT); // Device certificate
net.setPrivateKey(AWS_CERT_PRIVATE); // Private key
```

**Stored in**: `config/secrets.h` (gitignored)

### 10.2 Credential Management

```
secrets.h (never committed):
  - WiFi SSID/password
  - AWS IoT endpoint
  - Device certificate
  - Private key

secrets.h.template (committed):
  - Placeholder values
  - Instructions for users
```

### 10.3 Data Sanitization

- No WiFi password in logs
- No certificate/key in debug output
- Task names sanitized (no PII)
- Shadow updates validated before publish

---

## 11. Error Handling Architecture

### 11.1 Error Categories

| Category | Severity | Action |
|----------|----------|--------|
| WiFi disconnect | Warning | Auto-reconnect, queue updates |
| MQTT disconnect | Warning | Auto-reconnect, queue updates |
| NTP failure | Warning | Retry with exponential backoff |
| NVS write fail | Error | Retry 3×, log error, continue |
| OOM (Out of Memory) | Critical | Reset device after safe state save |
| Watchdog timeout | Critical | Reset device (Core 1 hang) |

### 11.2 Recovery Strategies

**WiFi Reconnection**:
```
Disconnect detected
    → Wait 5 seconds
    → WiFi.reconnect()
    → Success? → Resume sync
    → Failure? → Wait 10 seconds, retry
    → Max 5 retries, then wait 60 seconds
```

**MQTT Reconnection**:
```
Disconnect detected
    → WiFi connected?
    ├─ No → Wait for WiFi
    └─ Yes → MQTT.connect()
               → Success? → Re-subscribe, publish queue
               → Failure? → Exponential backoff (5s, 10s, 20s, 40s)
```

**NVS Write Failure**:
```
preferences.putUInt() fails
    → Retry with 100ms delay
    → Retry 3× total
    → Still failing? → Log error, skip persist
    → Continue operation (use RAM value)
```

---

## 12. Testing Architecture

### 12.1 Unit Test Structure

```
test/
├── test_state_machine/
│   └── test_transitions.cpp    // All state transitions
├── test_sequence/
│   └── test_classic_cycle.cpp  // 4-session logic
├── test_statistics/
│   └── test_calculations.cpp   // Streaks, averages
└── test_time_manager/
    └── test_sync.cpp            // NTP, drift correction
```

**Test Framework**: PlatformIO Native (runs on PC, not device)

**Coverage Target**: 70% for core logic

### 12.2 Integration Test Scenarios

1. **Complete Sequence**: 4-session classic cycle
2. **Pause/Resume**: Time preservation test
3. **Gyro Gestures**: All orientations, accuracy measurement
4. **Statistics**: 7-day accumulation test
5. **WiFi Disconnect**: Offline queue behavior
6. **Battery Drain**: 100% → 0% runtime test
7. **Deep Sleep**: Wake and resume test

### 12.3 Hardware-in-Loop Testing

**Required Hardware**:
- M5Stack Core2 device
- USB-C cable (power + serial monitor)
- WiFi network (2.4 GHz)
- AWS IoT account
- Optional: HMI module (encoder)

**Test Procedure**:
1. Flash firmware via PlatformIO
2. Monitor serial output (115200 baud)
3. Manually trigger all states
4. Verify behavior matches specification
5. Measure battery life with multimeter

---

## 13. Deployment Architecture

### 13.1 Build Pipeline

```
Developer makes changes
    │
    ├─ Edit code in VSCode
    ├─ PlatformIO: Check syntax
    │
    ▼
Run unit tests
    │
    ├─ PlatformIO Test (native)
    ├─ 70% coverage target
    │
    ▼
Build firmware
    │
    ├─ platformio run
    ├─ Compile: ESP32 Arduino framework
    ├─ Link: M5Unified + dependencies
    ├─ Generate: .bin file (~2MB)
    │
    ▼
Flash device
    │
    ├─ platformio run --target upload
    ├─ Upload via USB serial (921600 baud)
    │
    ▼
Monitor & test
    │
    └─ Serial monitor: Verify logs, test features
```

### 13.2 Version Management

```
Firmware version: 2.0.0 (semantic versioning)
  - Major: Breaking changes
  - Minor: New features
  - Patch: Bug fixes
```

**Version stored in**:
- Firmware: `#define FIRMWARE_VERSION "2.0.0"`
- Shadow: `"v": "2.0.0"` field
- Build metadata: Git commit hash

---

## 14. Performance Characteristics

### 14.1 Timing Targets

| Metric | Target | Measured (Estimated) |
|--------|--------|----------------------|
| Frame time | <100ms | ~80ms |
| Touch latency | <100ms | ~50ms |
| Gyro detection | <500ms | ~300ms |
| MQTT connect | <15s (cold) | ~12s |
| MQTT reconnect | <2s (SSL resume) | ~1.5s |
| NTP sync | <2s | ~1.5s |
| NVS write | <50ms | ~20ms |

### 14.2 Resource Utilization

| Resource | Capacity | Usage (Target) | Headroom |
|----------|----------|----------------|----------|
| SRAM | 520KB | 200KB | 320KB (62%) |
| PSRAM | 8MB | 2MB | 6MB (75%) |
| Flash | 16MB | 12MB | 4MB (25%) |
| CPU Core 0 | 100% | 30% | 70% |
| CPU Core 1 | 100% | 10% | 90% |

---

## 15. Future Architecture Considerations

### 15.1 Scalability

**Current**: Single device
**Future**:
- Multi-device sync via cloud
- Device groups (home, office)
- Shared statistics

**Required Changes**:
- Shadow schema: Add device_id field
- Statistics: Aggregate across devices
- Config: Cloud-based sync

### 15.2 Extensibility

**Current**: Fixed feature set
**Future**:
- Plugin architecture for new modes
- Custom audio alerts (upload via app)
- Third-party integrations (Notion, Todoist)

**Required Changes**:
- Module loader framework
- Asset upload via WiFi
- Integration API

### 15.3 Maintainability

**Current**: Monorepo
**Future**:
- Component libraries (reusable)
- CI/CD pipeline (GitHub Actions)
- Automated testing

**Required Changes**:
- Refactor into libraries
- Add GitHub Actions workflow
- Mock hardware for CI tests

---

**Document Version**: 1.0
**Last Updated**: 2025-01-03
**Status**: APPROVED
**Next Review**: After Phase 2 (local timer) completion
