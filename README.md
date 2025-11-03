# M5 Pomodoro Timer v2

A productivity timer for M5Stack Core2 implementing the Pomodoro Technique with gyro-based gesture controls, cloud sync, and advanced power management.

## Features

- **Classic Pomodoro Sequence**: 4-session cycle (25-5-25-5-25-5-25-15) + Study mode (45-15)
- **Gyro Gesture Controls**: Rotate to start/pause, lay flat to resume, double-shake to stop
- **90-Day Statistics**: Track completed sessions, work time, and streaks
- **Three Power Modes**: Performance (2h), Balanced (4-6h), Battery Saver (8-12h)
- **Cloud Sync**: AWS IoT Device Shadow with Toggl and Google Calendar integration
- **Offline-First**: Full functionality without cloud connectivity

## Hardware

- **Platform**: M5Stack Core2
- **MCU**: ESP32-D0WDQ6-V3 (dual-core @ 240MHz)
- **Memory**: 520KB SRAM, 8MB PSRAM, 16MB Flash
- **Display**: 320×240 IPS touchscreen
- **IMU**: MPU6886 (6-axis gyro/accelerometer)
- **Battery**: 390mAh LiPo (4+ hour target)

## Quick Start

### Prerequisites

- [PlatformIO Core](https://platformio.org/) 6.1+
- USB Type-C cable
- M5Stack Core2 device

### Build and Upload

```bash
# Clone repository
git clone <repository-url>
cd 021.M5_pomodoro_v2

# Build firmware
pio run

# Upload to device
pio run --target upload

# Monitor serial output
pio device monitor
```

### Configuration

Edit `src/utils/Config.h` to configure:
- WiFi credentials
- AWS IoT endpoint and certificates
- Toggl API token
- Timer durations
- Power mode defaults

## Project Structure

```
021.M5_pomodoro_v2/
├── docs/                   # Comprehensive documentation
│   ├── 00-PROJECT-OVERVIEW.md
│   ├── 01-TECHNICAL-REQUIREMENTS.md
│   ├── 02-ARCHITECTURE.md
│   ├── 03-STATE-MACHINE.md
│   ├── 04-GYRO-CONTROL.md
│   ├── 05-STATISTICS-SYSTEM.md
│   ├── 06-POWER-MANAGEMENT.md
│   ├── 07-UI-DESIGN.md
│   ├── 08-CLOUD-SYNC.md
│   ├── 09-BUILD-DEPLOY.md
│   └── ADRs/               # Architecture Decision Records
├── src/                    # Source code
│   ├── core/               # Timer logic, statistics, settings
│   ├── input/              # Gyro, touch input
│   ├── ui/                 # Screens and widgets
│   ├── feedback/           # LED, audio
│   ├── network/            # Cloud sync, NTP
│   └── utils/              # Logger, config, helpers
├── test/                   # Unit tests
├── data/                   # Filesystem (audio, images)
├── platformio.ini          # PlatformIO configuration
├── partitions.csv          # Flash partition table
└── CLAUDE.md               # Development guide

```

## Development

### Architecture

- **Dual-Core FreeRTOS**: Core 0 (UI/input), Core 1 (logic/network)
- **M5Unified Library**: v0.1.13+ (M5Core2 deprecated)
- **State Machine**: 5 states, 8 events, classic Pomodoro sequence
- **Power Management**: Light sleep with 1s RTC wake (94% power reduction)
- **NVS Storage**: Statistics and config persist across reboots and OTA updates

See [docs/02-ARCHITECTURE.md](docs/02-ARCHITECTURE.md) for details.

### Testing

```bash
# Run unit tests
pio test

# Run specific test
pio test -f test_state_machine

# Generate coverage report
pio test --with-coverage
```

### Documentation

- **Planning Docs**: 10 comprehensive documents (~40,000 words)
- **ADRs**: 8 architecture decision records
- **Code Docs**: Doxygen-style inline documentation

Run `doxygen Doxyfile` to generate HTML documentation.

## Usage

### Basic Operation

1. **Start Timer**: Rotate device ±60° (gyro gesture)
2. **Pause**: Rotate device again during active session
3. **Resume**: Lay device flat for 1 second
4. **Stop**: Double-shake or press STOP button

### Power Modes

- **Performance**: Maximum responsiveness, ~2 hour battery
- **Balanced** (default): Good UX, 4-6 hour battery
- **Battery Saver**: Extended runtime, 8-12 hours

Change in Settings → Power Mode

### Cloud Sync

Configure AWS IoT credentials in `src/utils/Config.h`. Device automatically syncs:
- Timer state on session start/end
- Statistics on session completion
- Toggl time entries (optional)
- Google Calendar events (optional)

## License

[Add license information]

## Credits

Built with:
- [M5Unified](https://github.com/m5stack/M5Unified) - M5Stack library
- [ArduinoJson](https://arduinojson.org/) - JSON parsing
- [PubSubClient](https://github.com/knolleary/pubsubclient) - MQTT client
- [PlatformIO](https://platformio.org/) - Build system

## Version

**v2.0.0** - Complete rebuild with improved architecture, power management, and cloud sync.

See [CHANGELOG.md](CHANGELOG.md) for full version history.
