# ADR-001: Use M5Unified Library Instead of M5Core2

**Status**: Accepted

**Date**: 2025-12-15

**Decision Makers**: Development Team

## Context

M5Stack Core2 v1 used the `M5Core2` library (v0.1.x), which provided device-specific APIs for display, touch, IMU, power management, and peripherals. However, M5Stack has since deprecated the `M5Core2` library in favor of the unified `M5Unified` library, which supports multiple M5Stack devices with a consistent API.

### Options Considered

1. **Continue using M5Core2 library (v0.1.9)**
   - Pros: Existing v1 code compatible, no migration needed
   - Cons: Deprecated, no bug fixes, limited community support, missing features

2. **Migrate to M5Unified library (v0.1.13+)**
   - Pros: Active development, cross-device compatibility, better performance, improved M5GFX rendering, bug fixes
   - Cons: Requires code refactoring, API changes

3. **Use raw ESP-IDF APIs**
   - Pros: Maximum control, no abstraction overhead
   - Cons: Significantly more complex, loss of M5Stack-specific optimizations, reinventing the wheel

## Decision

**We will use the M5Unified library (v0.1.13 or later) for M5 Pomodoro Timer v2.**

### Rationale

1. **Active Maintenance**: M5Unified is actively maintained by M5Stack with regular bug fixes and feature additions
2. **Performance**: M5GFX (included in M5Unified) provides faster rendering with dirty rectangle support and sprite optimization
3. **Future-Proofing**: M5Unified supports Core2 and future M5Stack devices, enabling potential multi-device support
4. **Community Support**: Larger community adoption, better documentation, more examples
5. **Simplified API**: More consistent and intuitive API than legacy M5Core2
6. **Bug Fixes**: M5Core2 has known issues with touch calibration and IMU initialization that are fixed in M5Unified

### Key API Changes

**Initialization**:
```cpp
// OLD (M5Core2)
#include <M5Core2.h>
M5.begin(true, true, true, true);  // LCD, SD, Serial, I2C

// NEW (M5Unified)
#include <M5Unified.h>
auto cfg = M5.config();
cfg.clear_display = true;
cfg.internal_imu = true;
cfg.internal_rtc = true;
cfg.internal_spk = true;
cfg.internal_mic = false;
M5.begin(cfg);
```

**Display**:
```cpp
// OLD
M5.Lcd.fillScreen(BLACK);
M5.Lcd.setTextColor(WHITE);
M5.Lcd.drawString("Hello", 10, 10);

// NEW
M5.Display.fillScreen(TFT_BLACK);
M5.Display.setTextColor(TFT_WHITE);
M5.Display.drawString("Hello", 10, 10);
```

**Touch**:
```cpp
// OLD
TouchPoint_t pos = M5.Touch.getPressPoint();
if (pos.x >= 0 && pos.y >= 0) { /* touched */ }

// NEW
auto touch = M5.Touch.getDetail();
if (touch.wasPressed()) {
    int16_t x = touch.x;
    int16_t y = touch.y;
}
```

**IMU (MPU6886)**:
```cpp
// OLD
M5.IMU.Init();
float ax, ay, az, gx, gy, gz;
M5.IMU.getAccelData(&ax, &ay, &az);
M5.IMU.getGyroData(&gx, &gy, &gz);

// NEW
M5.Imu.begin();
float ax, ay, az, gx, gy, gz;
M5.Imu.getAccel(&ax, &ay, &az);
M5.Imu.getGyro(&gx, &gy, &gz);
```

**Power (AXP192)**:
```cpp
// OLD
M5.Axp.SetLcdVoltage(2800);
float batVoltage = M5.Axp.GetBatVoltage();

// NEW
M5.Power.setExtPowerOutput(true);
int batLevel = M5.Power.getBatteryLevel();
```

### Migration Effort

- Estimated effort: 2-4 hours
- Impact areas: Initialization, display rendering, touch input, IMU access, power management
- Risk: Low (well-documented migration path, community support)

## Consequences

### Positive

- **Improved Performance**: M5GFX provides 2-3x faster rendering with dirty rectangles
- **Better Touch Handling**: More reliable touch detection with debouncing built-in
- **Unified API**: Consistent naming across all modules (Display, Touch, Imu, Power)
- **Active Support**: Bug fixes and feature additions from M5Stack team
- **Future Compatibility**: Easy to port to M5Stack Core3 or other devices if needed

### Negative

- **Breaking Change**: v1 code requires refactoring (acceptable, no backward compatibility required)
- **Learning Curve**: Team needs to learn new API (mitigated by good documentation)
- **Dependency**: Reliance on M5Stack's library updates (acceptable, stable release cycle)

### Neutral

- **Library Size**: M5Unified is ~50KB larger than M5Core2 (acceptable given 16MB Flash)

## Implementation

### Dependencies

```ini
[env:m5stack-core2]
lib_deps =
    m5stack/M5Unified@^0.1.13
```

### Initialization Pattern

```cpp
// src/main.cpp
#include <M5Unified.h>

void setup() {
    auto cfg = M5.config();
    cfg.clear_display = true;
    cfg.internal_imu = true;
    cfg.internal_rtc = true;
    cfg.internal_spk = false;  // Using external speaker via DAC
    cfg.internal_mic = false;  // Not used
    M5.begin(cfg);

    // Update loop rate
    M5.update();  // Call in loop() for touch/button updates
}

void loop() {
    M5.update();  // Must call to update touch/button state
    // ... rest of loop
}
```

### Testing

- [ ] Verify display rendering (colors, fonts, sprites)
- [ ] Verify touch input (single touch, multi-touch, gestures)
- [ ] Verify IMU access (accelerometer, gyroscope)
- [ ] Verify power management (battery level, voltage, charging status)
- [ ] Verify speaker output (if enabled)
- [ ] Verify SD card access (if used)

## References

- M5Unified GitHub: https://github.com/m5stack/M5Unified
- M5Unified Documentation: https://docs.m5stack.com/en/api/m5unified
- M5GFX Documentation: https://github.com/m5stack/M5GFX
- Migration Guide: https://docs.m5stack.com/en/arduino/m5unified/migration_guide

## Related ADRs

- ADR-002: Dual-Core FreeRTOS Architecture
- ADR-007: Classic Pomodoro Sequence Implementation

## Revision History

- 2025-12-15: Initial version, decision to use M5Unified v0.1.13
