# ADR-009: NVS Statistics Storage

**Status**: Accepted
**Date**: 2025-01-10
**Deciders**: Claude (AI), User
**Related**: [ADR-006](ADR-006-nvs-statistics-storage.md), [MP-14](https://chistokhin.youtrack.cloud/issue/MP-14), [MP-79](https://chistokhin.youtrack.cloud/issue/MP-79)

---

## Context

With SD card support being added to M5 Pomodoro v2 (MP-70), a decision is needed about where to store 90-day statistics data:

1. **NVS (Non-Volatile Storage)** - ESP32 internal flash with wear-leveling
2. **SD Card** - Removable FAT32 storage
3. **Hybrid** - NVS primary + SD backup

**Current Implementation**: Statistics stored in NVS (per ADR-006, MP-14 complete)

**Storage Requirements**:
- 90 days × 28 bytes/day = 2,520 bytes (~2.5KB)
- Circular buffer (oldest day overwritten after 90 days)
- Daily write frequency (once per midnight, ~365 writes/year)

**Related Components**:
- `src/core/Statistics.h/cpp` - Already uses NVS Preferences API
- SD card integration (MP-70) - Now available for alternative storage

---

## Decision

**Keep 90-day statistics in NVS (do NOT move to SD card)**

Statistics will remain stored in ESP32 internal NVS using the Preferences library.

---

## Rationale

### Why NVS is Superior for Statistics

1. **Critical Data Reliability**
   - Statistics are core functionality, should always be available
   - SD card is removable - user may remove for configuration changes
   - NVS is internal - cannot be accidentally removed
   - **Requirement**: Statistics must persist across SD card removal

2. **Wear-Leveling Optimized for Flash**
   - ESP32 NVS has built-in wear-leveling
   - Daily writes (~365/year) are low-frequency for NVS
   - SD cards have lower write endurance than ESP32 flash
   - NVS designed for this exact use case

3. **Small Data Size (2.5KB)**
   - Statistics data is tiny compared to NVS capacity (~500KB available)
   - SD card overkill for 2.5KB storage
   - NVS access is faster (no SPI overhead)
   - No filesystem fragmentation concerns

4. **Atomic Transactions**
   - NVS Preferences API provides atomic updates
   - SD card writes require manual sync/flush
   - Power loss during SD write = corruption risk
   - NVS more resilient to unexpected power loss

5. **Simplified Architecture**
   - Statistics already implemented in NVS (MP-14 complete, tested)
   - No code changes required
   - SD card reserved for user-editable data (config, audio, certs)
   - Clear separation: NVS = device state, SD = user data

---

## Consequences

### Positive

- ✅ **Reliability**: Statistics always available, even if SD removed
- ✅ **Performance**: Faster access (no SPI overhead)
- ✅ **Wear-Leveling**: NVS optimized for infrequent writes
- ✅ **Atomic Updates**: Built-in transaction support
- ✅ **No Code Changes**: Existing implementation works perfectly
- ✅ **Clear Architecture**: NVS = device data, SD = user data

### Negative

- ❌ **No PC Access**: Cannot view statistics without device (until export feature)
- ❌ **Limited Backup**: No automatic backup to SD card
- ❌ **Size Limit**: Restricted to NVS size (2.5KB sufficient, but not expandable)

### Neutral

- ⚖️ **Export Feature Deferred**: Statistics export to SD/cloud postponed to v2.1+
- ⚖️ **90-Day Limit Enforced**: NVS size naturally enforces circular buffer design

---

## Alternatives Considered

### Alternative 1: Move Statistics to SD Card

**Pros**:
- Larger storage capacity (could extend beyond 90 days)
- User can view statistics files on PC
- Easy backup (copy SD card)

**Cons**:
- **CRITICAL ISSUE**: Lose statistics if SD removed
- Slower access (SPI overhead for every read/write)
- File corruption risk (power loss during write)
- Filesystem fragmentation over time
- Overkill for 2.5KB data
- Requires code refactoring (MP-14 already complete)

**Decision**: ❌ Rejected - reliability concerns outweigh benefits

### Alternative 2: Hybrid (NVS Primary + SD Backup)

**Pros**:
- Best of both worlds (reliability + backup)
- User can view backup files on PC
- Graceful degradation if either storage fails

**Cons**:
- Increased complexity (dual-storage management)
- Write overhead (double writes on every update)
- Sync issues (NVS and SD may diverge)
- SD backup not automatically restored if NVS cleared

**Decision**: ❌ Rejected - complexity not justified for 2.5KB data
**Future Consideration**: Export feature (manual, user-triggered) is simpler

### Alternative 3: Cloud-Only Storage

**Pros**:
- Unlimited storage
- Accessible from anywhere
- Automatic backup

**Cons**:
- Requires constant WiFi connectivity
- Privacy concerns
- Complex failure modes
- Not suitable for primary storage

**Decision**: ❌ Rejected - NVS primary is essential
**Note**: Cloud sync is complementary (MP-36), not replacement

---

## Implementation Notes

### Current NVS Schema (MP-14)

```cpp
// Namespace: "stats"
// Keys: "day_YYYY_MM_DD" -> StatisticsEntry (28 bytes)
struct StatisticsEntry {
    uint32_t completed_work;      // Completed work sessions
    uint32_t completed_rest;      // Completed rest sessions
    uint32_t skipped;             // Skipped sessions
    uint32_t total_time_work;     // Total work time (seconds)
    uint32_t total_time_rest;     // Total rest time (seconds)
    uint16_t longest_streak;      // Longest streak (sessions)
    uint16_t day_of_year;         // Day identifier
};
```

### No Changes Required

- `Statistics.cpp` already uses NVS (Preferences API)
- Circular buffer implemented (oldest day overwritten)
- Tested and working (MP-14 verified)

### Future Export Feature (v2.1+)

When user requests statistics export:
1. Read all 90 days from NVS
2. Generate CSV or JSON file on SD card
3. User copies file from SD to PC
4. Optional: Cloud upload for analysis

**Export Format** (proposed):
```csv
Date,WorkSessions,RestSessions,Skipped,WorkTime,RestTime,Streak
2025-01-01,12,11,1,300,55,5
2025-01-02,10,10,0,250,50,10
...
```

---

## Related Decisions

- **ADR-006**: NVS statistics storage (original decision, MP-14 implementation)
- **MP-70**: SD card infrastructure (config, audio, certs - NOT statistics)
- **MP-36**: Cloud sync (Device Shadow syncs aggregated stats, not daily details)

---

## References

- [ESP32 NVS Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html)
- [Arduino Preferences Library](https://github.com/espressif/arduino-esp32/tree/master/libraries/Preferences)
- ADR-006: Original statistics storage decision
- MP-14: Statistics implementation (complete)
- MP-70: SD card infrastructure (complete)

---

## Changelog

| Date       | Author | Change                                  |
|------------|--------|-----------------------------------------|
| 2025-01-10 | Claude | Initial ADR - Keep statistics in NVS   |

---

**Status**: Accepted ✅
**Next Review**: v2.1 planning (when export feature prioritized)
