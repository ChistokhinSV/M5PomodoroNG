# M5 Pomodoro Timer NG

A modern Pomodoro timer for M5Stack Core2 with dual-core architecture, configurable timer modes, and statistics tracking.

## Current Status

**Active Development** - Core timer functionality, UI navigation, and settings management implemented. Cloud sync and power optimization planned for future releases.

## Implemented Features

- **Configurable Timer Modes**: Classic (25/5/15/4), Study (45/15/30/2), and Custom presets
- **Multi-Screen UI**: Main timer, Statistics, Settings (5 pages), and Pause screens
- **Hardware Button Control**: Three physical buttons for navigation and timer control
- **Statistics Tracking**: 90-day session history with NVS persistence
- **Visual Feedback**: LED animations, progress bar, session breadcrumbs
- **Audio Alerts**: WAV playback for session transitions
- **Haptic Feedback**: Vibration motor for button presses and timer events
- **Dual-Core Architecture**: FreeRTOS tasks on Core 0 (UI) and Core 1 (Network - skeleton)

## Hardware

- **Platform**: M5Stack Core2
- **MCU**: ESP32-D0WDQ6-V3 (dual-core @ 240MHz)
- **Memory**: 520KB SRAM, 8MB PSRAM, 16MB Flash
- **Display**: 320×240 IPS touchscreen
- **Battery**: 390mAh LiPo
- **Audio**: NS4168 I2S amplifier
- **Haptic**: Vibration motor
- **LEDs**: SK6812 RGB (10× addressable)

## Quick Start

### Prerequisites

- [PlatformIO Core](https://platformio.org/) 6.1+
- USB Type-C cable
- M5Stack Core2 device

### Build and Upload

```bash
# Clone repository
git clone https://github.com/ChistokhinSV/M5PomodoroNG.git
cd M5PomodoroNG

# Build firmware
pio run

# Upload to device
pio run --target upload

# Monitor serial output
pio device monitor --baud 115200
```

## Project Structure

```
M5PomodoroNG/
├── docs/                       # Architecture documentation
│   └── ADRs/                   # Architecture Decision Records (9 ADRs)
├── src/                        # Source code
│   ├── core/                   # Timer logic, statistics, config
│   │   ├── Config.h/cpp        # NVS configuration management
│   │   ├── PomodoroSequence.h/cpp
│   │   ├── TimerStateMachine.h/cpp
│   │   └── Statistics.h/cpp
│   ├── hardware/               # Hardware abstraction
│   │   ├── AudioPlayer.h/cpp
│   │   ├── LEDController.h/cpp
│   │   ├── HapticController.h/cpp
│   │   └── PowerManager.h/cpp
│   ├── ui/                     # Screens and widgets
│   │   ├── ScreenManager.h/cpp
│   │   ├── screens/
│   │   │   ├── MainScreen.h/cpp
│   │   │   ├── StatsScreen.h/cpp
│   │   │   ├── SettingsScreen.h/cpp
│   │   │   └── PauseScreen.h/cpp
│   │   └── widgets/
│   │       ├── ProgressBar.h/cpp
│   │       ├── StatusBar.h/cpp
│   │       ├── SequenceIndicator.h/cpp
│   │       └── HardwareButtonBar.h/cpp
│   ├── tasks/                  # FreeRTOS tasks
│   │   ├── UITask.cpp
│   │   └── NetworkTask.cpp
│   └── main.cpp
├── platformio.ini              # PlatformIO configuration
├── CLAUDE.md                   # Development guidelines
└── README.md                   # This file
```

## Architecture

### Dual-Core FreeRTOS

- **Core 0 (UI Task)**: Rendering, input handling, state machine, audio, LEDs
- **Core 1 (Network Task)**: WiFi, MQTT, NTP sync (skeleton implementation)

### Key Design Patterns

- **Single Responsibility**: Config as single source of truth (see ADR-009)
- **State Machine**: 5 states (IDLE, ACTIVE, PAUSED, SESSION_COMPLETE, CYCLE_COMPLETE)
- **Callback Pattern**: UI → ScreenManager → Config → PomodoroSequence
- **M5Unified Library**: v0.2.10 (M5Core2 library deprecated)

See [docs/ADRs/](docs/ADRs/) for architectural decision records.

## Usage

### Hardware Buttons

- **BtnA (Left)**: Start/Pause/Resume timer
- **BtnB (Center)**: Navigate to Stats screen
- **BtnC (Right)**: Navigate to Settings screen

### Settings

Navigate to Settings screen (BtnC) to configure:

**Page 0 - Timer Durations**:
- Work Duration (1-90 min)
- Short Break (1-30 min)
- Long Break (5-60 min)
- Mode Presets: Classic / Study / Custom

**Page 1 - Timer Options**:
- Sessions Before Long Break (1-10)
- Number of Cycles (1-10)
- Auto-start Breaks (ON/OFF)
- Auto-start Work (ON/OFF)

**Page 2 - Display**:
- Screen Brightness (10-255)
- Show Seconds (ON/OFF)
- Screen Timeout (5-300 sec)

**Page 3 - Audio & Haptic**:
- Sound Effects (ON/OFF)
- Volume (0-100)
- Haptic Feedback (ON/OFF)

**Page 4 - Power**:
- Auto Sleep (ON/OFF)
- Sleep After (1-60 min)
- Wake on Rotation (ON/OFF)
- Min Battery Threshold (5-50%)

### Statistics

Press BtnB to view daily/weekly/monthly stats:
- Completed work sessions
- Total work time
- Current streak
- Weekly productivity chart

## Development

### Dependencies

Managed by PlatformIO:
- M5Unified v0.2.10
- ArduinoJson v7.4.2
- PubSubClient v2.8.0
- NTPClient v3.2.1
- FastLED v3.10.3
- Preferences (ESP32 NVS)

### Key Components

**State Machine** (src/core/TimerStateMachine.h):
- IDLE → ACTIVE → SESSION_COMPLETE → (next session) → CYCLE_COMPLETE
- Handles START, PAUSE, RESUME, STOP, TIMEOUT events
- Triggers LED animations and audio alerts

**Configuration** (src/core/Config.h):
- NVS persistence for all settings
- Single source of truth (see ADR-009)
- Automatic reload to PomodoroSequence on changes

**Statistics** (src/core/Statistics.h):
- 90-day rolling window in NVS
- Daily/weekly/monthly aggregation
- Streak tracking (current and longest)

### Building

```bash
# Clean build
pio run --target clean

# Verbose build output
pio run -v

# Upload with serial monitor
pio run -t upload && pio device monitor --baud 115200
```

## Planned Features

- [ ] Gyro gesture controls (rotation, shake, tilt)
- [ ] Deep sleep mode (ESP32 light sleep)
- [ ] AWS IoT Device Shadow sync
- [ ] Toggl time tracking integration
- [ ] Google Calendar integration
- [ ] OTA firmware updates
- [ ] Power mode optimization (Performance/Balanced/Battery Saver)

## License

MIT License - see [LICENSE](LICENSE) file for details.

## Credits

Built with:
- [M5Unified](https://github.com/m5stack/M5Unified) - M5Stack hardware library
- [ArduinoJson](https://arduinojson.org/) - JSON parsing
- [FastLED](https://github.com/FastLED/FastLED) - LED control
- [PlatformIO](https://platformio.org/) - Build system and development platform

## Contributing

This project follows Single Responsibility and callback-based architecture patterns. See [CLAUDE.md](CLAUDE.md) for development guidelines and [docs/ADRs/](docs/ADRs/) for architectural decisions.

## Version

**v0.2.0-alpha** - Active development. Core timer and UI implemented, cloud sync and power optimization pending.

**Latest Commit**: Refactored settings to single-responsibility architecture
