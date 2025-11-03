# Build and Deployment Specification

## 1. Overview

This document specifies the build system, dependencies, compilation process, and deployment procedures for M5 Pomodoro Timer v2. The project uses PlatformIO for dependency management and cross-platform builds.

### Build Tool

**PlatformIO Core 6.1+**
- Reasons: Superior dependency management, reproducible builds, integrated testing
- VS Code integration via PlatformIO IDE extension
- CLI support for CI/CD pipelines

### Target Platform

```
Board:       M5Stack Core2
Framework:   Arduino (ESP-IDF underneath)
MCU:         ESP32-D0WDQ6-V3
Flash:       16MB
PSRAM:       8MB
Upload:      USB Type-C (CP2104 serial)
```

## 2. Project Structure

```
C:\HOME\1.SCRIPTS\021.M5_pomodoro_v2\
├── platformio.ini              # PlatformIO configuration
├── README.md                   # Project documentation
├── CLAUDE.md                   # Development guide
├── CHANGELOG.md                # Version history
├── .gitignore                  # Git ignore rules
│
├── src/                        # Source files
│   ├── main.cpp                # Entry point
│   │
│   ├── core/                   # Core logic
│   │   ├── TimerStateMachine.h/cpp
│   │   ├── Statistics.h/cpp
│   │   ├── Settings.h/cpp
│   │   └── PowerManager.h/cpp
│   │
│   ├── input/                  # Input handling
│   │   ├── GyroController.h/cpp
│   │   ├── TouchInput.h/cpp
│   │   └── InputManager.h/cpp
│   │
│   ├── ui/                     # UI components
│   │   ├── UIManager.h/cpp
│   │   ├── screens/
│   │   │   ├── Screen.h
│   │   │   ├── MainScreen.h/cpp
│   │   │   ├── StatsScreen.h/cpp
│   │   │   └── SettingsScreen.h/cpp
│   │   └── widgets/
│   │       ├── StatusBar.h/cpp
│   │       ├── TimerDisplay.h/cpp
│   │       ├── ProgressBar.h/cpp
│   │       ├── SequenceIndicator.h/cpp
│   │       ├── Button.h/cpp
│   │       └── StatsChart.h/cpp
│   │
│   ├── feedback/               # Feedback systems
│   │   ├── LEDController.h/cpp
│   │   └── AudioPlayer.h/cpp
│   │
│   ├── network/                # Cloud sync
│   │   ├── CloudSync.h/cpp
│   │   ├── NTPClient.h/cpp
│   │   └── OfflineQueue.h/cpp
│   │
│   └── utils/                  # Utilities
│       ├── Logger.h/cpp
│       ├── Config.h
│       └── Helpers.h/cpp
│
├── include/                    # Header files (if needed)
│
├── lib/                        # Private libraries
│   └── README                  # Placeholder
│
├── test/                       # Unit tests
│   ├── test_state_machine.cpp
│   ├── test_statistics.cpp
│   ├── test_gyro.cpp
│   ├── test_power_manager.cpp
│   └── test_cloud_sync.cpp
│
├── data/                       # SPIFFS/LittleFS filesystem
│   ├── audio/
│   │   ├── beep.wav
│   │   ├── complete.wav
│   │   └── alert.wav
│   └── images/
│       └── logo.png
│
├── docs/                       # Documentation
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
│   └── ADRs/
│       ├── ADR-001-m5unified-library.md
│       ├── ADR-002-dual-core-architecture.md
│       ├── ADR-003-freertos-synchronization.md
│       ├── ADR-004-gyro-polling-strategy.md
│       ├── ADR-005-light-sleep-during-timer.md
│       ├── ADR-006-nvs-statistics-storage.md
│       ├── ADR-007-classic-pomodoro-sequence.md
│       └── ADR-008-power-mode-design.md
│
└── scripts/                    # Build/deploy scripts
    ├── generate_certs.py       # AWS IoT cert generator
    ├── provision.py            # Device provisioning
    └── upload_filesystem.py    # SPIFFS/LittleFS upload
```

## 3. PlatformIO Configuration

### platformio.ini

```ini
; M5 Pomodoro Timer v2 - PlatformIO Configuration

[platformio]
default_envs = m5stack-core2

[env:m5stack-core2]
platform = espressif32@6.5.0
board = m5stack-core2
framework = arduino

; Build flags
build_flags =
    -DCORE_DEBUG_LEVEL=3
    -DBOARD_HAS_PSRAM
    -mfix-esp32-psram-cache-issue
    -DARDUINO_M5STACK_CORE2
    -DM5UNIFIED_PC_SUPPORT=0
    ; Version info
    -DFIRMWARE_VERSION=\"2.0.0\"
    -DBUILD_DATE=\"$UNIX_TIME\"
    ; Features
    -DENABLE_CLOUD_SYNC=1
    -DENABLE_TOGGL=1
    -DENABLE_GCAL=1
    -DENABLE_AUDIO=1
    -DENABLE_LED=1

; Board configuration
board_build.partitions = partitions.csv
board_build.filesystem = littlefs
board_build.f_cpu = 240000000L
board_build.f_flash = 80000000L
board_build.flash_mode = qio
board_build.psram_type = qspi

; Upload configuration
upload_speed = 921600
monitor_speed = 115200
monitor_filters =
    esp32_exception_decoder
    time
    colorize

; Dependencies
lib_deps =
    m5stack/M5Unified@^0.1.13
    bblanchon/ArduinoJson@^7.0.3
    knolleary/PubSubClient@^2.8.0
    arduino-libraries/NTPClient@^3.2.1
    ; Testing framework
    google/googletest@^1.14.0

; Extra scripts
extra_scripts =
    pre:scripts/pre_build.py
    post:scripts/post_build.py

; Debugging
debug_tool = esp-prog
debug_init_break = tbreak setup

; Test configuration
test_framework = googletest
test_build_src = yes
```

### Custom Partitions (partitions.csv)

```csv
# Name,     Type, SubType, Offset,   Size,     Flags
nvs,        data, nvs,     0x9000,   0x5000,
otadata,    data, ota,     0xe000,   0x2000,
app0,       app,  ota_0,   0x10000,  0x300000,
app1,       app,  ota_1,   0x310000, 0x300000,
spiffs,     data, spiffs,  0x610000, 0x9E0000,
coredump,   data, coredump,0xFF0000, 0x10000,
```

**Explanation**:
- `nvs`: 20KB for settings, statistics, credentials
- `otadata`: 8KB for OTA metadata
- `app0`/`app1`: 3MB each for dual OTA partitions
- `spiffs`: ~9.9MB for audio files, images, logs
- `coredump`: 64KB for crash dumps (debugging)

**Total**: 16MB Flash

## 4. Dependencies

### Core Libraries

```ini
[env:m5stack-core2]
lib_deps =
    ; M5Stack Unified Library (includes Core2 support)
    m5stack/M5Unified@^0.1.13
        ; Includes: M5GFX, M5Unified, touch, IMU, power management

    ; JSON parsing (for cloud sync)
    bblanchon/ArduinoJson@^7.0.3
        ; Size: ~10KB code, ~2KB RAM
        ; Used for: Shadow documents, settings serialization

    ; MQTT client (for AWS IoT)
    knolleary/PubSubClient@^2.8.0
        ; Size: ~6KB code, ~512 bytes RAM
        ; Used for: Device Shadow sync

    ; NTP client (for time sync)
    arduino-libraries/NTPClient@^3.2.1
        ; Size: ~3KB code, ~256 bytes RAM
        ; Used for: Epoch timestamps

    ; Testing framework (test builds only)
    google/googletest@^1.14.0
        ; Not included in production builds
```

### Built-in ESP32 Libraries

```cpp
#include <WiFi.h>              // ESP32 WiFi
#include <WiFiClientSecure.h>  // TLS support
#include <nvs_flash.h>         // Non-volatile storage
#include <nvs.h>               // NVS API
#include <esp_sleep.h>         // Sleep modes
#include <esp_task_wdt.h>      // Watchdog timer
#include <freertos/FreeRTOS.h> // FreeRTOS
#include <freertos/task.h>     // Task management
#include <freertos/queue.h>    // Queues
#include <freertos/semphr.h>   // Semaphores
#include <time.h>              // Time functions
```

### Version Pinning

**Important**: Pin exact versions to ensure reproducible builds.

```ini
; ✅ Good (pinned)
m5stack/M5Unified@0.1.13

; ❌ Bad (unpinned, may break)
m5stack/M5Unified
```

## 5. Build Process

### Step-by-Step Build

```bash
# 1. Install PlatformIO Core (if not installed)
pip install -U platformio

# 2. Navigate to project directory
cd C:\HOME\1.SCRIPTS\021.M5_pomodoro_v2

# 3. Clean previous builds (optional)
pio run --target clean

# 4. Build project
pio run

# Expected output:
# RAM:   [==        ]  18.2% (used 59428 bytes from 327680 bytes)
# Flash: [====      ]  42.7% (used 670234 bytes from 1572864 bytes)
# Building .pio\build\m5stack-core2\firmware.bin

# 5. Upload to device
pio run --target upload

# 6. Monitor serial output
pio device monitor
```

### Build Targets

```bash
# Build only (no upload)
pio run

# Build and upload
pio run --target upload

# Build and upload filesystem (LittleFS)
pio run --target uploadfs

# Clean build artifacts
pio run --target clean

# Run unit tests
pio test

# Run specific test
pio test -f test_state_machine

# Build with verbose output
pio run -v

# Build for debugging
pio run --target debug
```

### Pre-Build Script (scripts/pre_build.py)

```python
Import("env")
import time

# Add build timestamp
env.Append(CPPDEFINES=[
    ("BUILD_TIMESTAMP", f"\\\"{int(time.time())}\\\"")
])

# Check for required files
import os
required_files = [
    "src/main.cpp",
    "platformio.ini",
    "partitions.csv"
]

for file in required_files:
    if not os.path.exists(file):
        raise Exception(f"Required file missing: {file}")

print("✓ Pre-build checks passed")
```

### Post-Build Script (scripts/post_build.py)

```python
Import("env")
import shutil
import os

# Copy firmware to releases directory
def copy_firmware(source, target, env):
    firmware_src = str(target[0])
    firmware_dst = "releases/firmware-v2.0.0.bin"

    os.makedirs("releases", exist_ok=True)
    shutil.copy(firmware_src, firmware_dst)

    print(f"✓ Firmware copied to {firmware_dst}")
    print(f"  Size: {os.path.getsize(firmware_dst)} bytes")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", copy_firmware)
```

## 6. Filesystem Preparation

### Audio Files

```bash
# Convert audio to WAV format (8-bit, 16kHz, mono)
ffmpeg -i input.mp3 -ar 16000 -ac 1 -acodec pcm_u8 data/audio/beep.wav
ffmpeg -i complete.mp3 -ar 16000 -ac 1 -acodec pcm_u8 data/audio/complete.wav
ffmpeg -i alert.mp3 -ar 16000 -ac 1 -acodec pcm_u8 data/audio/alert.wav
```

### Image Files

```bash
# Convert images to PNG (320x240, 16-bit color)
magick convert logo.jpg -resize 320x240 -depth 16 data/images/logo.png
```

### Upload Filesystem

```bash
# Build filesystem image
pio run --target buildfs

# Upload to device
pio run --target uploadfs

# Expected output:
# Building LittleFS image from 'data' directory to .pio/build/m5stack-core2/littlefs.bin
# Total: 1024KB, Used: 125KB (12%)
# Uploading filesystem...
# Done!
```

## 7. Testing

### Unit Tests

```bash
# Run all tests
pio test

# Run specific test file
pio test -f test_state_machine

# Run tests with verbose output
pio test -v

# Run tests on device (not host)
pio test --upload-port COM3
```

### Test Structure (test/test_state_machine.cpp)

```cpp
#include <gtest/gtest.h>
#include "core/TimerStateMachine.h"

// Test fixture
class TimerStateMachineTest : public ::testing::Test {
protected:
    void SetUp() override {
        stateMachine = new TimerStateMachine();
    }

    void TearDown() override {
        delete stateMachine;
    }

    TimerStateMachine* stateMachine;
};

// Test cases
TEST_F(TimerStateMachineTest, InitialState) {
    EXPECT_EQ(stateMachine->getState(), PomodoroState::STOPPED);
}

TEST_F(TimerStateMachineTest, StartTimer) {
    stateMachine->handleEvent(TimerEvent::START);
    EXPECT_EQ(stateMachine->getState(), PomodoroState::POMODORO);
    EXPECT_EQ(stateMachine->getRemainingSeconds(), 1500);  // 25 min
}

TEST_F(TimerStateMachineTest, PauseTimer) {
    stateMachine->handleEvent(TimerEvent::START);
    delay(2000);  // Run for 2 seconds
    stateMachine->handleEvent(TimerEvent::PAUSE);

    EXPECT_EQ(stateMachine->getState(), PomodoroState::PAUSED);
    EXPECT_LT(stateMachine->getRemainingSeconds(), 1500);
}

TEST_F(TimerStateMachineTest, CompleteSequence) {
    // Session 1: Pomodoro + Short Rest
    stateMachine->handleEvent(TimerEvent::START);
    stateMachine->simulateExpire();
    EXPECT_EQ(stateMachine->getState(), PomodoroState::SHORT_REST);

    stateMachine->simulateExpire();
    EXPECT_EQ(stateMachine->getState(), PomodoroState::STOPPED);
    EXPECT_EQ(stateMachine->getSessionNumber(), 2);
}
```

### Coverage Report

```bash
# Generate coverage report (requires lcov)
pio test --with-coverage

# View coverage in browser
genhtml coverage.info -o coverage_report
start coverage_report/index.html
```

**Target**: 70%+ code coverage for core modules

## 8. Deployment

### Initial Flash (USB)

```bash
# Step 1: Connect M5Stack Core2 via USB Type-C

# Step 2: Check device port
pio device list
# Expected: COM3 (Windows) or /dev/ttyUSB0 (Linux)

# Step 3: Build and upload firmware
pio run --target upload

# Step 4: Upload filesystem
pio run --target uploadfs

# Step 5: Monitor serial output
pio device monitor --port COM3 --baud 115200
```

### OTA Update (Over-The-Air)

**Setup** (one-time):
```cpp
// src/network/OTAUpdate.h
#include <ArduinoOTA.h>

void setupOTA() {
    ArduinoOTA.setHostname("m5-pomodoro");
    ArduinoOTA.setPassword("pomodoro2024");  // Change in production!

    ArduinoOTA.onStart([]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "firmware" : "filesystem";
        LOG_INFO("OTA update started: " + type);
        M5.Display.clear();
        M5.Display.drawString("Updating...", 160, 120);
    });

    ArduinoOTA.onEnd([]() {
        LOG_INFO("OTA update complete");
        M5.Display.drawString("Update complete!", 160, 140);
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        uint8_t percent = (progress / (total / 100));
        M5.Display.progressBar(20, 160, 280, 20, percent);
    });

    ArduinoOTA.onError([](ota_error_t error) {
        LOG_ERROR("OTA error: " + String(error));
    });

    ArduinoOTA.begin();
}

void loop() {
    ArduinoOTA.handle();
    // ... rest of loop
}
```

**Deploy via OTA**:
```bash
# Find device on network
pio device list --mdns

# Upload firmware via OTA
pio run --target upload --upload-port m5-pomodoro.local

# Or specify IP address
pio run --target upload --upload-port 192.168.1.100
```

### AWS IoT Jobs (Future)

```python
# scripts/deploy_ota_job.py
import boto3
import json

iot_client = boto3.client('iot')

# Create OTA job
job_document = {
    "operation": "update",
    "firmwareUrl": "https://s3.amazonaws.com/firmware/m5-pomodoro-v2.0.1.bin",
    "version": "2.0.1",
    "checksum": "sha256:abc123..."
}

response = iot_client.create_job(
    jobId='m5-pomodoro-ota-v2.0.1',
    targets=[
        'arn:aws:iot:us-east-1:123456789:thing/m5-pomodoro-*'
    ],
    document=json.dumps(job_document),
    targetSelection='SNAPSHOT'
)

print(f"OTA job created: {response['jobArn']}")
```

## 9. Provisioning

### Device Provisioning Workflow

```
1. Flash firmware (USB)
2. Device boots → AP mode (if no WiFi configured)
3. User connects to "M5-Pomodoro-XXXXXX" AP
4. Captive portal → WiFi configuration
5. User enters WiFi credentials + AWS IoT certs
6. Device reboots → connects to WiFi
7. Device provisions to AWS IoT Core
8. Done!
```

### Provisioning Script (scripts/provision.py)

```python
#!/usr/bin/env python3
import serial
import time
import json

def provision_device(port, wifi_ssid, wifi_password, aws_endpoint, certs):
    """
    Provision M5Stack device with WiFi and AWS IoT credentials
    """
    ser = serial.Serial(port, 115200, timeout=5)
    time.sleep(2)  # Wait for device to boot

    # Step 1: Enter provisioning mode
    ser.write(b'PROVISION\n')
    response = ser.readline().decode().strip()
    if response != "READY":
        raise Exception(f"Device not ready: {response}")

    # Step 2: Send WiFi credentials
    wifi_config = json.dumps({
        "ssid": wifi_ssid,
        "password": wifi_password
    })
    ser.write(f"WIFI:{wifi_config}\n".encode())
    response = ser.readline().decode().strip()
    if response != "WIFI_OK":
        raise Exception(f"WiFi config failed: {response}")

    # Step 3: Send AWS IoT endpoint
    ser.write(f"AWS_ENDPOINT:{aws_endpoint}\n".encode())
    response = ser.readline().decode().strip()
    if response != "AWS_OK":
        raise Exception(f"AWS config failed: {response}")

    # Step 4: Send certificates (base64 encoded)
    import base64
    for cert_type in ['ca', 'crt', 'key']:
        with open(certs[cert_type], 'rb') as f:
            cert_data = base64.b64encode(f.read()).decode()
        ser.write(f"CERT_{cert_type.upper()}:{cert_data}\n".encode())
        response = ser.readline().decode().strip()
        if response != f"CERT_{cert_type.upper()}_OK":
            raise Exception(f"Cert upload failed: {response}")

    # Step 5: Finalize provisioning
    ser.write(b'PROVISION_DONE\n')
    response = ser.readline().decode().strip()
    if response != "PROVISIONED":
        raise Exception(f"Provisioning failed: {response}")

    print("✓ Device provisioned successfully!")
    ser.close()

# Usage
if __name__ == "__main__":
    provision_device(
        port="COM3",
        wifi_ssid="MyNetwork",
        wifi_password="MyPassword",
        aws_endpoint="xxxxxx.iot.us-east-1.amazonaws.com",
        certs={
            'ca': 'certs/AmazonRootCA1.pem',
            'crt': 'certs/device-cert.pem.crt',
            'key': 'certs/device-private.pem.key'
        }
    )
```

### AWS IoT Certificate Generation

```bash
# Generate device certificates
aws iot create-keys-and-certificate \
    --set-as-active \
    --certificate-pem-outfile device-cert.pem.crt \
    --public-key-outfile device-public.pem.key \
    --private-key-outfile device-private.pem.key

# Download Amazon Root CA
curl -o AmazonRootCA1.pem https://www.amazontrust.com/repository/AmazonRootCA1.pem

# Attach policy to certificate
aws iot attach-policy \
    --policy-name M5PomodoroPolicy \
    --target arn:aws:iot:us-east-1:123456789:cert/xxxxx

# Create Thing
aws iot create-thing --thing-name m5-pomodoro-A1B2C3

# Attach certificate to Thing
aws iot attach-thing-principal \
    --thing-name m5-pomodoro-A1B2C3 \
    --principal arn:aws:iot:us-east-1:123456789:cert/xxxxx
```

## 10. Continuous Integration

### GitHub Actions Workflow (.github/workflows/build.yml)

```yaml
name: Build and Test

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main ]

jobs:
  build:
    runs-on: ubuntu-latest

    steps:
    - uses: actions/checkout@v3

    - name: Set up Python
      uses: actions/setup-python@v4
      with:
        python-version: '3.11'

    - name: Install PlatformIO
      run: |
        python -m pip install --upgrade pip
        pip install platformio

    - name: Build firmware
      run: pio run

    - name: Run tests
      run: pio test

    - name: Upload firmware artifact
      uses: actions/upload-artifact@v3
      with:
        name: firmware
        path: .pio/build/m5stack-core2/firmware.bin

    - name: Upload filesystem artifact
      uses: actions/upload-artifact@v3
      with:
        name: filesystem
        path: .pio/build/m5stack-core2/littlefs.bin

  test:
    runs-on: ubuntu-latest

    steps:
    - uses: actions/checkout@v3

    - name: Set up Python
      uses: actions/setup-python@v4
      with:
        python-version: '3.11'

    - name: Install PlatformIO
      run: pip install platformio

    - name: Run unit tests
      run: pio test

    - name: Generate coverage report
      run: |
        pio test --with-coverage
        pip install lcov_cobertura
        lcov_cobertura coverage.info -o coverage.xml

    - name: Upload coverage to Codecov
      uses: codecov/codecov-action@v3
      with:
        file: ./coverage.xml
        fail_ci_if_error: true
```

### Build Matrix (Optional)

```yaml
strategy:
  matrix:
    build_type: [debug, release]
    enable_cloud: [0, 1]

env:
  BUILD_TYPE: ${{ matrix.build_type }}
  ENABLE_CLOUD_SYNC: ${{ matrix.enable_cloud }}
```

## 11. Release Process

### Version Numbering

**Semantic Versioning**: `MAJOR.MINOR.PATCH`

```
2.0.0 - Initial v2 release
2.0.1 - Bug fix (statistics calculation)
2.1.0 - New feature (custom themes)
3.0.0 - Breaking change (new API)
```

### Release Checklist

- [ ] Update version in `platformio.ini` (`FIRMWARE_VERSION`)
- [ ] Update `CHANGELOG.md` with release notes
- [ ] Run full test suite (`pio test`)
- [ ] Build release firmware (`pio run`)
- [ ] Test on physical device (full feature set)
- [ ] Tag release in Git (`git tag v2.0.0`)
- [ ] Push to GitHub (`git push --tags`)
- [ ] Create GitHub Release with firmware binary
- [ ] Update documentation (README.md)
- [ ] Deploy to AWS IoT (OTA job, if applicable)

### CHANGELOG.md Format

```markdown
# Changelog

## [2.0.0] - 2025-12-15

### Added
- Classic Pomodoro sequence tracking (4 sessions)
- Study mode (45-15 intervals)
- 90-day statistics with streak tracking
- Gyro-based gesture controls
- Three power modes (Performance, Balanced, Battery Saver)
- AWS IoT Device Shadow sync
- Toggl time tracking integration
- Google Calendar event creation

### Changed
- Migrated from M5Core2 library to M5Unified
- Refactored to dual-core FreeRTOS architecture
- Improved battery life (4+ hours in Balanced mode)

### Removed
- Backward compatibility with v1 shadow schema
- Support for M5Core2 library (deprecated)

## [1.0.0] - 2024-06-01
- Initial release (v1)
```

## 12. Troubleshooting

### Build Errors

**Error**: `fatal error: M5Unified.h: No such file or directory`
```bash
# Solution: Install dependencies
pio lib install
```

**Error**: `region 'iram0_0_seg' overflowed by 12345 bytes`
```ini
; Solution: Reduce code in IRAM, move to Flash
[env:m5stack-core2]
build_flags =
    -DCORE_DEBUG_LEVEL=0  ; Disable debug logs
    -Os                   ; Optimize for size
```

**Error**: `Sketch uses 105% of program storage space`
```ini
; Solution: Increase partition size
board_build.partitions = partitions_large.csv
```

### Upload Errors

**Error**: `Failed to connect to ESP32: No serial data received`
```bash
# Solution 1: Hold RESET button during upload
# Solution 2: Check USB cable (data, not charge-only)
# Solution 3: Install CP2104 drivers
```

**Error**: `Timed out waiting for packet header`
```ini
; Solution: Reduce upload speed
[env:m5stack-core2]
upload_speed = 115200  ; (default: 921600)
```

### Runtime Errors

**Error**: Device crashes on boot
```bash
# Check serial output for stack trace
pio device monitor --filter esp32_exception_decoder

# Common causes:
# 1. Stack overflow → Increase stack size in FreeRTOS task
# 2. Heap exhaustion → Check memory leaks
# 3. Watchdog timeout → Add task delays
```

**Error**: WiFi won't connect
```cpp
// Enable WiFi debug logs
#define CORE_DEBUG_LEVEL 4  // VERBOSE

// Check WiFi status
WiFi.printDiag(Serial);
```

## 13. Memory Optimization

### Code Size Reduction

```ini
[env:m5stack-core2]
build_flags =
    -Os                          ; Optimize for size
    -ffunction-sections          ; Place functions in separate sections
    -fdata-sections              ; Place data in separate sections
    -Wl,--gc-sections            ; Garbage collect unused sections
    -DCORE_DEBUG_LEVEL=0         ; Disable debug logs (saves ~20KB)
    -DENABLE_AUDIO=0             ; Disable audio (optional, saves ~15KB)
```

### RAM Optimization

```cpp
// Use PROGMEM for constants
const char* STATES[] PROGMEM = {"STOPPED", "POMODORO", "SHORT_REST", "LONG_REST", "PAUSED"};

// Use F() macro for strings
Serial.println(F("Timer started"));

// Allocate large buffers in PSRAM
char* largeBuffer = (char*)ps_malloc(100000);
```

### Flash Optimization

```ini
; Use LittleFS instead of SPIFFS (smaller overhead)
board_build.filesystem = littlefs

; Compress filesystem
board_build.compress_filesystem = yes
```

## 14. Documentation Generation

### Doxygen Configuration

```bash
# Install Doxygen
choco install doxygen.install  # Windows
apt install doxygen            # Linux

# Generate documentation
doxygen Doxyfile

# View in browser
start docs/html/index.html
```

### Code Documentation Style

```cpp
/**
 * @file TimerStateMachine.h
 * @brief Finite state machine for Pomodoro timer
 * @author Claude (M5 Pomodoro v2)
 * @version 2.0.0
 * @date 2025-12-15
 */

/**
 * @class TimerStateMachine
 * @brief Manages timer states and transitions
 *
 * Implements a finite state machine with 5 states and 8 events.
 * Handles classic Pomodoro sequence (4 sessions) and Study mode (45-15).
 */
class TimerStateMachine {
public:
    /**
     * @brief Handle timer event and transition to new state
     * @param event The event to process
     * @return true if state changed, false otherwise
     */
    bool handleEvent(TimerEvent event);

    /**
     * @brief Get current timer state
     * @return Current state (STOPPED, POMODORO, SHORT_REST, LONG_REST, PAUSED)
     */
    PomodoroState getState() const { return currentState; }
};
```

## 15. Performance Profiling

### CPU Profiling

```cpp
// Enable FreeRTOS runtime stats
#define configGENERATE_RUN_TIME_STATS 1

void printTaskStats() {
    char statsBuffer[512];
    vTaskGetRunTimeStats(statsBuffer);
    Serial.println(statsBuffer);
}

// Output:
// Task          Runtime    %
// ui_task       2345       23%
// timer_task    1234       12%
// network_task  567        5%
// ...
```

### Memory Profiling

```cpp
void printMemoryStats() {
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
    Serial.printf("Largest free block: %d bytes\n", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    Serial.printf("Min free heap (since boot): %d bytes\n", ESP.getMinFreeHeap());
}
```

## 16. Related Documents

- [02-ARCHITECTURE.md](02-ARCHITECTURE.md) - System architecture
- [01-TECHNICAL-REQUIREMENTS.md](01-TECHNICAL-REQUIREMENTS.md) - Requirements
- [CLAUDE.md](../CLAUDE.md) - Development guide

## 17. References

- PlatformIO Documentation: https://docs.platformio.org/
- ESP32 Arduino Core: https://github.com/espressif/arduino-esp32
- M5Unified Library: https://github.com/m5stack/M5Unified
- ESP-IDF Programming Guide: https://docs.espressif.com/projects/esp-idf/
- FreeRTOS Documentation: https://www.freertos.org/Documentation/
