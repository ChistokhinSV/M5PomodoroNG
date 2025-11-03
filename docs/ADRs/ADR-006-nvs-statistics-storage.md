# ADR-006: NVS-Based Statistics Storage

**Status**: Accepted

**Date**: 2025-12-15

**Decision Makers**: Development Team

## Context

The M5 Pomodoro Timer needs to persist 90 days of statistics data (completed sessions, work time, streaks) across power cycles and firmware updates. We need to decide on a storage mechanism that balances:
- Persistence (survives reboot, firmware update)
- Performance (fast read/write)
- Wear leveling (Flash has limited write cycles)
- Size constraints (statistics data ~2.5KB)

### Storage Options on ESP32

1. **NVS (Non-Volatile Storage)**
   - Flash partition with wear leveling
   - Key-value store
   - Survives reboot and firmware update (OTA safe)
   - Typical size: 12-20KB

2. **SPIFFS/LittleFS**
   - Filesystem on Flash
   - File-based storage
   - Survives reboot, OTA depends on partition layout
   - Size: Flexible (typically MB range)

3. **RTC Memory**
   - 8KB fast RAM, powered by RTC
   - Survives deep sleep
   - Does NOT survive power loss
   - Very fast access

4. **External EEPROM/SD Card**
   - Unlimited writes (EEPROM) or high capacity (SD)
   - Requires additional hardware
   - Not available on M5Stack Core2 by default

### Requirements

1. Store 90 days of daily statistics (~2,550 bytes)
2. Persist across reboot and firmware updates (OTA)
3. Fast access (<10ms read, <50ms write)
4. Minimize Flash wear (target: <10,000 writes per year)
5. No external hardware dependencies

## Decision

**We will use NVS (Non-Volatile Storage) for all persistent statistics:**

- **Statistics data**: `pomodoro_stats` namespace
- **Configuration**: `pomodoro_config` namespace
- **Cloud credentials**: `cloud_creds` namespace (encrypted)

### Rationale

1. **OTA Safe**: NVS partition separate from app partitions, survives firmware updates
2. **Wear Leveling**: Built-in wear leveling algorithm distributes writes across Flash sectors
3. **Performance**: Fast access (~5ms read, ~20ms write), meets requirements
4. **Atomic Writes**: NVS guarantees atomic writes, no corruption risk
5. **Size Appropriate**: 2.5KB statistics fit comfortably in 15KB NVS partition
6. **Built-In**: No additional hardware or libraries needed, part of ESP-IDF

### Data Structures

```cpp
// Daily statistics (28 bytes per day)
struct DailyStats {
    uint32_t date;                    // YYYYMMDD (e.g., 20251215)
    uint8_t pomodoros_completed;      // 0-255
    uint8_t pomodoros_started;        // 0-255
    uint16_t total_work_minutes;      // 0-65535 (max 1092 hours per day)
    uint8_t interruptions;            // 0-255
    uint8_t longest_streak_today;     // 0-255
    uint32_t first_session_epoch;     // Unix timestamp
    uint32_t last_session_epoch;      // Unix timestamp
    uint8_t _padding[3];              // Align to 4-byte boundary
};  // 28 bytes

// Statistics container (2,550 bytes total)
struct Statistics {
    DailyStats daily[90];             // Circular buffer (2520 bytes)
    uint8_t buffer_head;              // Index of oldest entry
    uint16_t lifetime_pomodoros;      // Total pomodoros ever
    uint8_t current_streak_days;      // Current streak
    uint8_t best_streak_days;         // Best streak ever
    uint32_t total_work_minutes_lifetime;  // Total work time
    uint32_t last_updated;            // Last update epoch
    uint8_t _padding[6];              // Align to 4-byte boundary
};  // 2,550 bytes total
```

### NVS Schema

```
Namespace: "pomodoro_stats"
├─ Key: "stats_blob"    → Statistics struct (2,550 bytes)
├─ Key: "version"       → uint8_t (schema version)
└─ Key: "checksum"      → uint32_t (CRC32 checksum)

Namespace: "pomodoro_config"
├─ Key: "pomodoro_dur"  → uint8_t (minutes)
├─ Key: "short_rest"    → uint8_t (minutes)
├─ Key: "long_rest"     → uint8_t (minutes)
├─ Key: "study_mode"    → uint8_t (0/1)
├─ Key: "power_mode"    → uint8_t (0=PERF, 1=BAL, 2=SAVER)
├─ Key: "gyro_sens"     → uint8_t (30-90 degrees)
└─ Key: "gyro_wake"     → uint8_t (0/1)

Namespace: "cloud_creds"
├─ Key: "aws_endpoint"  → string (128 bytes)
├─ Key: "device_id"     → string (16 bytes)
├─ Key: "toggl_token"   → blob (128 bytes, encrypted)
└─ Key: "calendar_id"   → string (128 bytes)
```

## Implementation

### Initialization

```cpp
// src/core/Statistics.cpp
#include <nvs_flash.h>
#include <nvs.h>

class Statistics {
public:
    Statistics();
    ~Statistics();

    bool begin();
    void save();
    void load();

private:
    nvs_handle_t stats_handle;
    nvs_handle_t config_handle;
    StatisticsData data;
    uint32_t calculateChecksum();
};

bool Statistics::begin() {
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition truncated, erase and reinit
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Open statistics namespace
    err = nvs_open("pomodoro_stats", NVS_READWRITE, &stats_handle);
    if (err != ESP_OK) {
        LOG_ERROR("Failed to open NVS stats namespace");
        return false;
    }

    // Open config namespace
    err = nvs_open("pomodoro_config", NVS_READWRITE, &config_handle);
    if (err != ESP_OK) {
        LOG_ERROR("Failed to open NVS config namespace");
        return false;
    }

    // Load existing data
    load();

    LOG_INFO("Statistics initialized, loaded " + String(data.lifetime_pomodoros) + " lifetime Pomodoros");
    return true;
}
```

### Load from NVS

```cpp
void Statistics::load() {
    size_t required_size = sizeof(StatisticsData);

    // Read blob
    esp_err_t err = nvs_get_blob(stats_handle, "stats_blob", &data, &required_size);

    if (err == ESP_OK) {
        // Verify checksum
        uint32_t stored_checksum;
        nvs_get_u32(stats_handle, "checksum", &stored_checksum);

        uint32_t calculated_checksum = calculateChecksum();

        if (stored_checksum == calculated_checksum) {
            LOG_INFO("Statistics loaded from NVS, checksum OK");
        } else {
            LOG_ERROR("Statistics checksum mismatch, data corrupted!");
            initializeDefaults();
        }
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        // First boot, initialize defaults
        LOG_INFO("No statistics found in NVS, initializing defaults");
        initializeDefaults();
        save();
    } else {
        LOG_ERROR("Failed to load statistics: " + String(err));
        initializeDefaults();
    }
}
```

### Save to NVS

```cpp
void Statistics::save() {
    // Update timestamp
    data.last_updated = getCurrentEpoch();

    // Calculate checksum
    uint32_t checksum = calculateChecksum();

    // Write blob
    esp_err_t err = nvs_set_blob(stats_handle, "stats_blob", &data, sizeof(StatisticsData));
    if (err != ESP_OK) {
        LOG_ERROR("Failed to save statistics blob: " + String(err));
        return;
    }

    // Write checksum
    err = nvs_set_u32(stats_handle, "checksum", checksum);
    if (err != ESP_OK) {
        LOG_ERROR("Failed to save checksum: " + String(err));
        return;
    }

    // Commit changes
    err = nvs_commit(stats_handle);
    if (err != ESP_OK) {
        LOG_ERROR("Failed to commit NVS: " + String(err));
        return;
    }

    LOG_INFO("Statistics saved to NVS, checksum: " + String(checksum, HEX));
}

uint32_t Statistics::calculateChecksum() {
    // CRC32 checksum
    uint32_t crc = 0xFFFFFFFF;
    uint8_t* ptr = (uint8_t*)&data;

    for (size_t i = 0; i < sizeof(StatisticsData); i++) {
        crc ^= ptr[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc = crc >> 1;
            }
        }
    }

    return ~crc;
}
```

### Configuration Save/Load

```cpp
void Settings::saveToNVS() {
    nvs_set_u8(config_handle, "pomodoro_dur", pomodoroDuration);
    nvs_set_u8(config_handle, "short_rest", shortRestDuration);
    nvs_set_u8(config_handle, "long_rest", longRestDuration);
    nvs_set_u8(config_handle, "study_mode", studyMode ? 1 : 0);
    nvs_set_u8(config_handle, "power_mode", static_cast<uint8_t>(powerMode));
    nvs_set_u8(config_handle, "gyro_sens", gyroSensitivity);
    nvs_set_u8(config_handle, "gyro_wake", gyroWake ? 1 : 0);
    nvs_commit(config_handle);

    LOG_INFO("Settings saved to NVS");
}

void Settings::loadFromNVS() {
    uint8_t value;

    if (nvs_get_u8(config_handle, "pomodoro_dur", &value) == ESP_OK) {
        pomodoroDuration = value;
    }
    if (nvs_get_u8(config_handle, "short_rest", &value) == ESP_OK) {
        shortRestDuration = value;
    }
    // ... load other settings

    LOG_INFO("Settings loaded from NVS");
}
```

## Write Frequency Analysis

### Statistics Write Events

```
Per Session (25 minutes):
- Session start:     1 write (update today's stats)
- Session complete:  1 write (update today's stats + lifetime)
Total: 2 writes per 25-minute session

Daily (assuming 8 Pomodoros):
- 8 sessions × 2 writes = 16 writes per day

Yearly:
- 16 writes/day × 365 days = 5,840 writes per year
```

### Flash Wear Analysis

ESP32 Flash specifications:
- Flash endurance: ~100,000 write/erase cycles per sector
- NVS sector size: 4KB
- NVS wear leveling: Distributes writes across 3+ sectors

```
Wear calculation:
5,840 writes/year ÷ 3 sectors = 1,947 writes per sector per year

Lifespan:
100,000 cycles ÷ 1,947 writes/year ≈ 51 years
```

**Conclusion**: NVS wear is negligible, device will outlast Flash endurance by decades.

### Configuration Writes

```
Typical: <10 writes per year (user changes settings rarely)
Negligible impact on Flash wear
```

## Consequences

### Positive

- **Persistence**: Survives reboot, firmware update, and power loss
- **Performance**: Fast access (5ms read, 20ms write)
- **Reliability**: Wear leveling ensures >50 year lifespan
- **Atomic Writes**: No corruption risk, even if power lost during write
- **OTA Safe**: Statistics preserved across firmware updates
- **Simplicity**: Built-in ESP-IDF API, no external dependencies

### Negative

- **Size Limit**: NVS partition typically 12-20KB (acceptable for 2.5KB data)
- **Write Latency**: 20ms write blocks task (mitigated by writing on session end only)
- **Flash Wear**: Finite write cycles (mitigated by wear leveling, 50+ year lifespan)

### Neutral

- **Checksum Overhead**: +4 bytes per namespace (negligible)

## Migration Strategy (OTA Updates)

### Schema Versioning

```cpp
#define STATS_SCHEMA_VERSION 1

void Statistics::load() {
    uint8_t version;
    if (nvs_get_u8(stats_handle, "version", &version) == ESP_OK) {
        if (version != STATS_SCHEMA_VERSION) {
            LOG_WARN("Schema version mismatch: " + String(version) + " vs " + String(STATS_SCHEMA_VERSION));
            migrateSchema(version, STATS_SCHEMA_VERSION);
        }
    } else {
        // No version found, assume v1
        nvs_set_u8(stats_handle, "version", STATS_SCHEMA_VERSION);
    }
}

void Statistics::migrateSchema(uint8_t fromVersion, uint8_t toVersion) {
    if (fromVersion == 0 && toVersion == 1) {
        // Migrate from unversioned to v1
        LOG_INFO("Migrating statistics schema from v0 to v1");
        // ... migration logic
    }
    // Future migrations
}
```

### Factory Reset

```cpp
void Statistics::factoryReset() {
    // Erase all data
    nvs_erase_all(stats_handle);
    nvs_erase_all(config_handle);
    nvs_commit(stats_handle);
    nvs_commit(config_handle);

    // Reinitialize defaults
    initializeDefaults();
    save();

    LOG_INFO("Factory reset complete");
}
```

## Testing Checklist

- [ ] Verify statistics persist across reboot
- [ ] Verify statistics persist across firmware update (OTA)
- [ ] Test checksum validation (corrupt data → default initialization)
- [ ] Test migration from v0 to v1 schema
- [ ] Measure write latency (target: <50ms)
- [ ] Measure read latency (target: <10ms)
- [ ] Test power loss during write (no corruption)
- [ ] Test factory reset (erase all data)
- [ ] Verify NVS partition size sufficient for 90 days (2.5KB)
- [ ] Test concurrent access (mutex protection)

## Alternatives Considered

### Why Not SPIFFS/LittleFS?

SPIFFS would provide file-based storage, but:
1. **Complexity**: Requires filesystem mount, file open/close/read/write
2. **Performance**: ~50-100ms file writes (vs. 20ms NVS)
3. **OTA Risk**: Filesystem partition may be overwritten during OTA (depends on partition layout)
4. **Overkill**: 2.5KB data doesn't benefit from filesystem features

**Conclusion**: NVS is simpler and faster for small key-value data.

### Why Not RTC Memory?

RTC memory would provide fastest access, but:
1. **Volatile**: Lost on power loss or hard reset
2. **Size Limit**: 8KB total (shared with other RTC data)
3. **Not OTA Safe**: Cleared during firmware update

**Conclusion**: Unacceptable for persistent statistics.

### Why Not SD Card?

SD card would provide unlimited storage, but:
1. **Hardware**: Not included on M5Stack Core2 by default
2. **Power**: SD card consumes 20-50mA during access
3. **Reliability**: SD card can be removed, corrupted, or fail
4. **Overkill**: 2.5KB data doesn't justify SD card

**Conclusion**: NVS is built-in and sufficient.

## Future Enhancements

### Cloud Backup

```cpp
void Statistics::backupToCloud() {
    // Serialize statistics to JSON
    StaticJsonDocument<4096> doc;
    serializeStats(doc);

    // Upload to AWS IoT Shadow
    cloudSync.publishStatsBackup(doc);

    LOG_INFO("Statistics backed up to cloud");
}

void Statistics::restoreFromCloud() {
    // Download from AWS IoT Shadow
    StaticJsonDocument<4096> doc = cloudSync.fetchStatsBackup();

    // Deserialize and restore
    deserializeStats(doc);
    save();

    LOG_INFO("Statistics restored from cloud");
}
```

### Export to JSON

```cpp
String Statistics::exportToJSON() {
    StaticJsonDocument<4096> doc;
    serializeStats(doc);

    String json;
    serializeJson(doc, json);

    return json;
}
```

## References

- ESP-IDF NVS Guide: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html
- NVS Partition Table: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/partition-tables.html
- Flash Wear Leveling: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/flash-wear-levelling.html

## Related ADRs

- ADR-005: Light Sleep During Timer (no complex RTC memory persistence)
- ADR-007: Classic Pomodoro Sequence (session tracking stored in NVS)
- ADR-008: Power Mode Design (power mode setting stored in NVS)

## Revision History

- 2025-12-15: Initial version, NVS-based storage for statistics and configuration
