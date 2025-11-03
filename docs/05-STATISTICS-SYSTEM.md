# Statistics System - M5 Pomodoro v2

## Document Information
- **Version**: 1.0
- **Date**: 2025-01-03
- **Status**: Draft
- **Related**: [02-ARCHITECTURE.md](02-ARCHITECTURE.md), [03-STATE-MACHINE.md](03-STATE-MACHINE.md)

---

## 1. Overview

The M5 Pomodoro v2 statistics system tracks productivity over time, including daily pomodoro counts, completion rates, and consecutive day streaks. All data persists in NVS (Non-Volatile Storage) across power cycles and device resets.

**Key Features**:
- 90-day rolling history
- Daily pomodoro counts
- Streak tracking (consecutive days)
- Completion rate (finished vs interrupted)
- Weekly visualization (bar chart)
- Lifetime counter

---

## 2. Data Structures

### 2.1 Daily Statistics

```cpp
struct DailyStats {
    uint32_t date;                    // YYYYMMDD format (e.g., 20250103)
    uint8_t pomodoros_completed;      // Successfully finished sessions
    uint8_t pomodoros_started;        // Total started (completed + interrupted)
    uint16_t total_work_minutes;      // Actual work time (not including rests)
    uint8_t interruptions;            // Stopped mid-session count
    uint8_t longest_streak_today;     // Max consecutive completed in one day
    uint32_t first_session_epoch;     // Timestamp of first pomodoro of day
    uint32_t last_session_epoch;      // Timestamp of last pomodoro of day
};
```

**Size**: ~28 bytes per day

**Fields Explained**:
- **date**: Integer date (e.g., 20250103 for Jan 3, 2025), easy to compare
- **pomodoros_completed**: Only incremented on EXPIRE event (timer reaches 00:00)
- **pomodoros_started**: Incremented on START event
- **total_work_minutes**: Sum of completed work session durations (25 or 45 min)
- **interruptions**: Incremented on STOP event mid-session (not at expiration)
- **longest_streak_today**: Max consecutive completions without interruption
- **first_session_epoch**: UTC timestamp of first START today (for analytics)
- **last_session_epoch**: UTC timestamp of last completion (for daily summary)

### 2.2 Overall Statistics

```cpp
struct Statistics {
    DailyStats daily[90];             // Circular buffer, newest 90 days
    uint8_t buffer_head;              // Index of oldest entry (next to overwrite)
    uint16_t lifetime_pomodoros;      // All-time completed count
    uint8_t current_streak_days;      // Consecutive days with ≥1 completed
    uint8_t best_streak_days;         // Longest streak ever achieved
    uint32_t total_work_minutes_lifetime; // All-time work minutes
    uint32_t last_update_epoch;       // Last time stats were modified
};
```

**Size**: ~2,550 bytes total (90 × 28 bytes + metadata)

**Storage**: Fits comfortably in 15KB NVS partition

### 2.3 In-Memory Cache

```cpp
class StatisticsManager {
private:
    Statistics stats;           // Loaded from NVS at startup
    DailyStats* today_cache;    // Pointer to today's entry (fast access)
    bool dirty;                 // Modified since last NVS write
    SemaphoreHandle_t statsMutex; // Thread-safe access
};
```

---

## 3. NVS Storage Schema

### 3.1 Key Structure

**Namespace**: `pomodoro_stats`

| Key | Type | Size | Description |
|-----|------|------|-------------|
| `daily_buffer` | Blob | 2520 bytes | 90-day circular buffer (90 × 28 bytes) |
| `buffer_head` | UInt8 | 1 byte | Index of oldest entry |
| `lifetime` | UInt16 | 2 bytes | All-time pomodoro count |
| `best_streak` | UInt8 | 1 byte | Best streak ever |
| `current_streak` | UInt8 | 1 byte | Current consecutive days |
| `total_minutes` | UInt32 | 4 bytes | Lifetime work minutes |
| `last_update` | UInt32 | 4 bytes | Last modification epoch |

**Total NVS Usage**: ~2,532 bytes (fits in 15KB partition with room for config)

### 3.2 Storage Operations

**Load from NVS** (at startup):
```cpp
void StatisticsManager::loadFromNVS() {
    preferences.begin("pomodoro_stats", true); // Read-only

    // Load circular buffer
    size_t bufferSize = preferences.getBytes("daily_buffer", stats.daily, sizeof(stats.daily));
    if (bufferSize == 0) {
        // First boot, initialize empty
        memset(stats.daily, 0, sizeof(stats.daily));
    }

    // Load metadata
    stats.buffer_head = preferences.getUChar("buffer_head", 0);
    stats.lifetime_pomodoros = preferences.getUShort("lifetime", 0);
    stats.current_streak_days = preferences.getUChar("current_streak", 0);
    stats.best_streak_days = preferences.getUChar("best_streak", 0);
    stats.total_work_minutes_lifetime = preferences.getUInt("total_minutes", 0);

    preferences.end();

    // Find today's entry
    updateTodayCache();
}
```

**Save to NVS** (on change):
```cpp
void StatisticsManager::saveToNVS() {
    if (!dirty) return; // No changes, skip write

    if (xSemaphoreTake(statsMutex, pdMS_TO_TICKS(100))) {
        preferences.begin("pomodoro_stats", false); // Read-write

        // Write circular buffer
        preferences.putBytes("daily_buffer", stats.daily, sizeof(stats.daily));

        // Write metadata
        preferences.putUChar("buffer_head", stats.buffer_head);
        preferences.putUShort("lifetime", stats.lifetime_pomodoros);
        preferences.putUChar("current_streak", stats.current_streak_days);
        preferences.putUChar("best_streak", stats.best_streak_days);
        preferences.putUInt("total_minutes", stats.total_work_minutes_lifetime);
        preferences.putUInt("last_update", rtc.getEpoch());

        preferences.end();

        dirty = false;
        xSemaphoreGive(statsMutex);
    }
}
```

**Write Strategy**: Immediate write on change (not deferred), ensures no data loss

---

## 4. Circular Buffer Management

### 4.1 90-Day Rolling Window

**Concept**: Keep newest 90 days, overwrite oldest when day 91 arrives

**Buffer Layout**:
```
Index:    0    1    2   ...   88   89
        ┌────┬────┬────┬─────┬────┬────┐
        │Day1│Day2│Day3│ ... │D89 │D90 │
        └────┴────┴────┴─────┴────┴────┘
             ↑
        buffer_head = 1 (oldest entry)

Next day (day 91):
        ┌────┬────┬────┬─────┬────┬────┐
        │D91 │Day2│Day3│ ... │D89 │D90 │  ← Overwrote oldest
        └────┴────┴────┴─────┴────┴────┘
                  ↑
             buffer_head = 2 (new oldest)
```

### 4.2 Insertion Algorithm

```cpp
void StatisticsManager::addDay(uint32_t date) {
    // Find insertion point (overwrite oldest)
    uint8_t insertIdx = buffer_head;

    // Initialize new day
    stats.daily[insertIdx] = {
        .date = date,
        .pomodoros_completed = 0,
        .pomodoros_started = 0,
        .total_work_minutes = 0,
        .interruptions = 0,
        .longest_streak_today = 0,
        .first_session_epoch = 0,
        .last_session_epoch = 0
    };

    // Advance head pointer (circular)
    stats.buffer_head = (stats.buffer_head + 1) % 90;

    dirty = true;
}
```

### 4.3 Lookup by Date

```cpp
DailyStats* StatisticsManager::findDay(uint32_t date) {
    for (int i = 0; i < 90; i++) {
        if (stats.daily[i].date == date) {
            return &stats.daily[i];
        }
    }
    return nullptr; // Not in buffer (older than 90 days)
}
```

**Optimization**: Cache pointer to today's entry (updated at midnight)

---

## 5. Statistics Calculations

### 5.1 Increment Completed Pomodoro

**Trigger**: State machine EXPIRE event (timer reaches 00:00 from POMODORO state)

```cpp
void StatisticsManager::incrementCompleted(uint16_t duration_minutes) {
    if (xSemaphoreTake(statsMutex, portMAX_DELAY)) {
        // Get or create today's entry
        uint32_t today = getDateYYYYMMDD();
        DailyStats* day = today_cache;
        if (day == nullptr || day->date != today) {
            day = findDay(today);
            if (day == nullptr) {
                addDay(today);
                day = findDay(today);
            }
            today_cache = day;
        }

        // Increment counters
        day->pomodoros_completed++;
        day->total_work_minutes += duration_minutes;
        stats.lifetime_pomodoros++;
        stats.total_work_minutes_lifetime += duration_minutes;

        // Update streak tracking
        updateStreak();

        // Mark for NVS write
        dirty = true;

        xSemaphoreGive(statsMutex);

        // Trigger async NVS write
        saveToNVS();
    }
}
```

### 5.2 Increment Started Session

**Trigger**: State machine START event

```cpp
void StatisticsManager::incrementStarted() {
    if (xSemaphoreTake(statsMutex, portMAX_DELAY)) {
        DailyStats* day = getTodayOrCreate();
        day->pomodoros_started++;

        if (day->first_session_epoch == 0) {
            day->first_session_epoch = rtc.getEpoch();
        }

        dirty = true;
        xSemaphoreGive(statsMutex);
    }
}
```

### 5.3 Increment Interruption

**Trigger**: State machine STOP event (mid-session, not at expiration)

```cpp
void StatisticsManager::incrementInterruption() {
    if (xSemaphoreTake(statsMutex, portMAX_DELAY)) {
        DailyStats* day = getTodayOrCreate();
        day->interruptions++;
        dirty = true;
        xSemaphoreGive(statsMutex);
    }
}
```

### 5.4 Streak Calculation

**Logic**: Consecutive days with ≥1 completed pomodoro

```cpp
void StatisticsManager::updateStreak() {
    uint32_t today = getDateYYYYMMDD();
    uint32_t yesterday = getPreviousDateYYYYMMDD(today);

    DailyStats* todayStats = findDay(today);
    DailyStats* yesterdayStats = findDay(yesterday);

    if (todayStats && todayStats->pomodoros_completed == 1) {
        // First pomodoro today
        if (yesterdayStats && yesterdayStats->pomodoros_completed > 0) {
            // Yesterday had pomodoros, continue streak
            stats.current_streak_days++;
        } else {
            // Yesterday was zero, restart streak
            stats.current_streak_days = 1;
        }

        // Update best streak if exceeded
        if (stats.current_streak_days > stats.best_streak_days) {
            stats.best_streak_days = stats.current_streak_days;
        }
    }
}
```

**Streak Break**: Detected at midnight if yesterday = 0 pomodoros
```cpp
void StatisticsManager::checkStreakBreak() {
    uint32_t yesterday = getYesterdayDateYYYYMMDD();
    DailyStats* yesterdayStats = findDay(yesterday);

    if (yesterdayStats == nullptr || yesterdayStats->pomodoros_completed == 0) {
        // Streak broken
        stats.current_streak_days = 0;
    }
}
```

### 5.5 Completion Rate

**Calculation**: Percentage of started sessions that were completed (not interrupted)

```cpp
float StatisticsManager::getCompletionRate(uint32_t date) {
    DailyStats* day = findDay(date);
    if (day == nullptr || day->pomodoros_started == 0) {
        return 0.0; // No data
    }

    return (float)day->pomodoros_completed / (float)day->pomodoros_started * 100.0;
}
```

**Example**:
- Started: 10 sessions
- Completed: 8 sessions
- Interrupted: 2 sessions
- Completion rate: 8 / 10 × 100% = **80%**

### 5.6 Weekly Aggregate

**Purpose**: Sum pomodoros for current week (Mon-Sun) for bar chart

```cpp
struct WeeklyStats {
    uint8_t counts[7];  // Mon=0, Tue=1, ..., Sun=6
    uint8_t total;
};

WeeklyStats StatisticsManager::getThisWeek() {
    WeeklyStats week = {0};
    uint32_t today = getDateYYYYMMDD();

    // Find Monday of current week
    uint32_t monday = getMondayOfWeek(today);

    for (int i = 0; i < 7; i++) {
        uint32_t date = addDays(monday, i);
        DailyStats* day = findDay(date);
        if (day) {
            week.counts[i] = day->pomodoros_completed;
            week.total += day->pomodoros_completed;
        }
    }

    return week;
}
```

### 5.7 Average Calculation

**7-Day Rolling Average**:
```cpp
float StatisticsManager::get7DayAverage() {
    uint32_t today = getDateYYYYMMDD();
    uint16_t total = 0;
    uint8_t daysWithData = 0;

    for (int i = 0; i < 7; i++) {
        uint32_t date = subtractDays(today, i);
        DailyStats* day = findDay(date);
        if (day && day->date == date) {
            total += day->pomodoros_completed;
            daysWithData++;
        }
    }

    return daysWithData > 0 ? (float)total / (float)daysWithData : 0.0;
}
```

---

## 6. Midnight Rollover Handling

### 6.1 Date Change Detection

**Trigger**: TimeManager checks date every minute

```cpp
void TimeManager::checkMidnightRollover() {
    static uint32_t lastDate = 0;
    uint32_t currentDate = getDateYYYYMMDD();

    if (lastDate != 0 && currentDate != lastDate) {
        // Midnight passed!
        onMidnightEvent();
    }

    lastDate = currentDate;
}

void onMidnightEvent() {
    // Notify statistics manager
    statisticsManager.onDayChange();

    // Notify sequence tracker (reset daily count)
    pomodoroSequence.onDayChange();
}
```

### 6.2 Day Change Actions

```cpp
void StatisticsManager::onDayChange() {
    // Check if streak continues or breaks
    checkStreakBreak();

    // Create new day entry
    uint32_t today = getDateYYYYMMDD();
    addDay(today);

    // Reset today cache
    today_cache = findDay(today);

    // Save to NVS
    saveToNVS();
}
```

---

## 7. Display Statistics

### 7.1 Today's Summary

**UI Display** (badge on main screen):
```cpp
struct TodaySummary {
    uint8_t completed;      // E.g., 6
    uint8_t started;        // E.g., 8
    uint8_t streak_days;    // E.g., 3
};

TodaySummary StatisticsManager::getTodaySummary() {
    DailyStats* today = getTodayOrCreate();
    return {
        .completed = today->pomodoros_completed,
        .started = today->pomodoros_started,
        .streak_days = stats.current_streak_days
    };
}
```

**Display**: "Today: 6 🍅  Streak: 3 days"

### 7.2 Weekly Bar Chart

**Data for Stats Screen**:
```cpp
struct ChartData {
    const char* labels[7] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    uint8_t values[7];      // Pomodoro counts
    uint8_t max_value;      // For scaling bars
};

ChartData StatisticsManager::getWeeklyChart() {
    WeeklyStats week = getThisWeek();
    ChartData chart;

    // Copy counts
    memcpy(chart.values, week.counts, 7);

    // Find max for scaling
    chart.max_value = 0;
    for (int i = 0; i < 7; i++) {
        if (chart.values[i] > chart.max_value) {
            chart.max_value = chart.values[i];
        }
    }

    return chart;
}
```

**Rendering** (on StatsScreen):
```cpp
void StatsScreen::renderChart(ChartData& data) {
    int barWidth = 40;
    int maxHeight = 100;

    for (int i = 0; i < 7; i++) {
        int x = 10 + i * 45;
        int barHeight = data.max_value > 0 ?
            (data.values[i] * maxHeight / data.max_value) : 0;
        int y = 200 - barHeight;

        // Draw bar
        canvas.fillRect(x, y, barWidth, barHeight, TFT_BLUE);

        // Draw label
        canvas.drawString(data.labels[i], x + 20, 205);

        // Draw value
        canvas.drawNumber(data.values[i], x + 20, y - 15);
    }
}
```

### 7.3 Lifetime Totals

**Display** (bottom of stats screen):
```cpp
struct LifetimeStats {
    uint16_t total_pomodoros;
    uint32_t total_hours;       // Derived from total_work_minutes / 60
    uint8_t best_streak;
};

LifetimeStats StatisticsManager::getLifetimeStats() {
    return {
        .total_pomodoros = stats.lifetime_pomodoros,
        .total_hours = stats.total_work_minutes_lifetime / 60,
        .best_streak = stats.best_streak_days
    };
}
```

**Display**: "Total: 847 🍅  Hours: 353  Best Streak: 12 days"

---

## 8. Export & Backup

### 8.1 CSV Export (Future Enhancement)

**Purpose**: Export statistics for external analysis

**Format**:
```csv
Date,Completed,Started,Work Minutes,Interruptions,Completion Rate
20250101,5,6,125,1,83.3%
20250102,7,7,175,0,100.0%
20250103,4,6,100,2,66.7%
```

**Implementation**:
```cpp
String StatisticsManager::exportCSV() {
    String csv = "Date,Completed,Started,Work Minutes,Interruptions,Completion Rate\n";

    // Iterate buffer in chronological order
    for (int i = 0; i < 90; i++) {
        int idx = (buffer_head + i) % 90;
        DailyStats& day = stats.daily[idx];

        if (day.date == 0) continue; // Empty slot

        float rate = day.pomodoros_started > 0 ?
            (float)day.pomodoros_completed / day.pomodoros_started * 100.0 : 0.0;

        csv += String(day.date) + ",";
        csv += String(day.pomodoros_completed) + ",";
        csv += String(day.pomodoros_started) + ",";
        csv += String(day.total_work_minutes) + ",";
        csv += String(day.interruptions) + ",";
        csv += String(rate, 1) + "%\n";
    }

    return csv;
}
```

**Delivery**: Save to SD card or send via MQTT to cloud

### 8.2 Cloud Backup (Optional)

**Strategy**: Publish statistics summary to AWS IoT shadow

**Shadow Field**:
```json
{
  "state": {
    "reported": {
      "stats": {
        "today": 6,
        "streak": 3,
        "lifetime": 847,
        "last_7d": [5, 7, 6, 4, 8, 0, 0]
      }
    }
  }
}
```

**Sync Frequency**: Daily (at midnight or on WiFi connect)

---

## 9. Data Migration

### 9.1 Schema Versioning

**Version Field** (in NVS):
```cpp
#define STATS_SCHEMA_VERSION 1

void StatisticsManager::loadFromNVS() {
    uint8_t version = preferences.getUChar("schema_ver", 0);

    if (version == 0) {
        // First boot or pre-versioning, initialize fresh
        initializeEmpty();
        preferences.putUChar("schema_ver", STATS_SCHEMA_VERSION);
    } else if (version < STATS_SCHEMA_VERSION) {
        // Migrate from older schema
        migrateFromVersion(version);
        preferences.putUChar("schema_ver", STATS_SCHEMA_VERSION);
    }
}
```

### 9.2 Future Schema Changes

**Example**: Add "focus_quality" field (v2)

```cpp
void migrateFromVersion(uint8_t oldVersion) {
    if (oldVersion == 1) {
        // Migrate v1 → v2
        // Old: DailyStats without focus_quality
        // New: DailyStats with focus_quality (default 0)

        DailyStats oldBuffer[90];
        preferences.getBytes("daily_buffer", oldBuffer, sizeof(oldBuffer));

        for (int i = 0; i < 90; i++) {
            stats.daily[i] = oldBuffer[i];
            stats.daily[i].focus_quality = 0; // New field
        }

        // Save migrated data
        saveToNVS();
    }
}
```

---

## 10. Error Handling

### 10.1 NVS Write Failure

**Scenario**: Flash wear-out, corruption, or full partition

```cpp
bool StatisticsManager::saveToNVS() {
    preferences.begin("pomodoro_stats", false);

    // Attempt write with error checking
    size_t written = preferences.putBytes("daily_buffer", stats.daily, sizeof(stats.daily));
    if (written == 0) {
        logger.error("Stats: NVS write failed!");
        preferences.end();

        // Retry once after 100ms
        delay(100);
        preferences.begin("pomodoro_stats", false);
        written = preferences.putBytes("daily_buffer", stats.daily, sizeof(stats.daily));
        if (written == 0) {
            logger.error("Stats: NVS write retry failed, data not persisted!");
            // Continue in-memory, will retry next change
            return false;
        }
    }

    preferences.end();
    return true;
}
```

### 10.2 Corrupted Data Recovery

**Detection**: Invalid date (e.g., date > 29991231 or date < 20200101)

```cpp
void StatisticsManager::validateAndRepair() {
    bool repaired = false;

    for (int i = 0; i < 90; i++) {
        DailyStats& day = stats.daily[i];

        // Check for invalid date
        if (day.date > 29991231 || (day.date > 0 && day.date < 20200101)) {
            logger.warn("Stats: Invalid date %d at index %d, clearing", day.date, i);
            memset(&day, 0, sizeof(DailyStats));
            repaired = true;
        }

        // Check for impossible values
        if (day.pomodoros_completed > 50) { // >12 hours of work unlikely
            logger.warn("Stats: Suspicious pomodoro count %d, capping to 50", day.pomodoros_completed);
            day.pomodoros_completed = 50;
            repaired = true;
        }
    }

    if (repaired) {
        dirty = true;
        saveToNVS();
    }
}
```

### 10.3 Midnight Detection Failure

**Problem**: Device sleeping during midnight, misses rollover

**Solution**: Check on wake and create missed day entries

```cpp
void StatisticsManager::checkMissedDays() {
    uint32_t today = getDateYYYYMMDD();
    uint32_t lastUpdate = preferences.getUInt("last_update", 0);

    if (lastUpdate > 0) {
        uint32_t lastDate = epochToDateYYYYMMDD(lastUpdate);

        // Calculate days missed
        int daysMissed = dateDifference(lastDate, today) - 1;

        if (daysMissed > 0) {
            logger.info("Stats: Missed %d days, creating empty entries", daysMissed);

            for (int i = 1; i <= daysMissed; i++) {
                uint32_t missedDate = addDays(lastDate, i);
                if (findDay(missedDate) == nullptr) {
                    addDay(missedDate); // Create empty day
                }
            }

            // Streak is broken (missed days had 0 pomodoros)
            stats.current_streak_days = 0;
            saveToNVS();
        }
    }
}
```

---

## 11. Performance Optimization

### 11.1 Lazy NVS Writes

**Current**: Write immediately on change
**Alternative**: Batch writes every 5 minutes

```cpp
void StatisticsManager::scheduleSave() {
    dirty = true;
    // Don't write immediately, wait for ticker
}

// Ticker (every 5 minutes)
void saveTickerCallback() {
    if (statsManager.isDirty()) {
        statsManager.saveToNVS();
    }
}
```

**Trade-off**: Risk of data loss if device crashes, but reduces flash wear

### 11.2 Today Cache

**Optimization**: Cache pointer to today's entry, avoid repeated lookups

```cpp
DailyStats* today_cache = nullptr;
uint32_t cached_date = 0;

DailyStats* getTodayOrCreate() {
    uint32_t today = getDateYYYYMMDD();

    if (today_cache && cached_date == today) {
        return today_cache; // Fast path
    }

    // Slow path: lookup and cache
    today_cache = findDay(today);
    if (today_cache == nullptr) {
        addDay(today);
        today_cache = findDay(today);
    }
    cached_date = today;

    return today_cache;
}
```

**Benefit**: O(1) access to today's stats (most frequent operation)

---

## 12. Testing

### 12.1 Unit Tests

| Test ID | Scenario | Expected Result |
|---------|----------|-----------------|
| U1 | Increment completed | Counts increase, dirty flag set |
| U2 | Increment on day with no entry | New day created automatically |
| U3 | Streak calculation: consecutive days | Streak increments correctly |
| U4 | Streak break: zero pomodoros yesterday | Streak resets to 0 or 1 |
| U5 | Circular buffer full (day 91) | Oldest entry overwritten |
| U6 | Completion rate: 8/10 | Returns 80.0% |
| U7 | Weekly aggregate: partial week | Sums only available days |
| U8 | Midnight rollover | New day created, streak updated |

### 12.2 Integration Tests

| Test ID | Scenario | Expected Result |
|---------|----------|-----------------|
| I1 | Complete 7 pomodoros over 7 days | Streak = 7, lifetime = 7 |
| I2 | Complete 5 pomodoros, reboot | Stats persist, counts match |
| I3 | Complete 100 pomodoros over 100 days | Circular buffer contains newest 90 |
| I4 | Zero pomodoros for 2 days | Streak = 0 |
| I5 | NVS write failure simulation | Retries, continues in-memory |

### 12.3 Stress Tests

| Test ID | Scenario | Expected Result |
|---------|----------|-----------------|
| S1 | 1000 rapid increments | No corruption, final count = 1000 |
| S2 | Reboot during NVS write | No corruption (atomic writes) |
| S3 | 365 days of data (exceeds 90) | Buffer maintains newest 90 only |

---

## 13. User Interface Integration

### 13.1 Main Screen Badge

**Location**: Top-right corner
**Content**: Today's count (e.g., "6 🍅")
**Update**: Real-time on every completion

```cpp
void MainScreen::updateTodayBadge() {
    TodaySummary summary = statsManager.getTodaySummary();
    canvas.drawString(String(summary.completed) + " 🍅", 280, 10);
}
```

### 13.2 Stats Screen Full View

**Layout**:
```
┌─────────────────────────────────┐
│ [← Back]      Statistics         │
├─────────────────────────────────┤
│ Today: 6 🍅    Streak: 3 days   │
├─────────────────────────────────┤
│ This Week                        │
│  Mon ▓▓▓▓▓ 5                    │
│  Tue ▓▓▓▓▓▓▓ 7                  │
│  Wed ▓▓▓▓▓▓ 6                   │
│  Thu ▓▓▓▓ 4                     │
│  Fri ▓▓ 2                       │
│  Sat ░░░░ 0                     │
│  Sun ░░░░ 0                     │
├─────────────────────────────────┤
│ Total: 847 🍅  Avg: 6.2/day     │
│ Work Time: 353h  Best: 12 days  │
└─────────────────────────────────┘
```

**Update**: On screen enter, refresh from stats manager

---

## 14. Future Enhancements

### 14.1 Advanced Analytics

**Time-of-Day Analysis**:
- Most productive hours (morning/afternoon/evening)
- Best day of week for focus

**Work Pattern Recognition**:
- Typical session length
- Average interruption rate per day
- Correlation between streak and completion rate

### 14.2 Gamification

**Achievements**:
- 🏆 "First 10" - Complete 10 lifetime pomodoros
- 🏆 "Week Warrior" - 7-day streak
- 🏆 "Century" - Complete 100 lifetime pomodoros
- 🏆 "Unstoppable" - 30-day streak

**Levels**:
- Level 1: 0-50 pomodoros
- Level 2: 51-100 pomodoros
- Level 3: 101-500 pomodoros
- Level 4: 501+ pomodoros

### 14.3 Trends & Insights

**Weekly Comparison**:
- This week vs last week (+15% or -10%)
- Monthly trend graph

**Predictive**:
- "On track for X pomodoros this week"
- "Best day for productivity: Tuesday"

---

**Document Version**: 1.0
**Last Updated**: 2025-01-03
**Status**: APPROVED
**Implementation**: MP-14 (Statistics), MP-18 (Stats Screen)
