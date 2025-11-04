#include "Statistics.h"
#include <Arduino.h>
#include <sys/time.h>

Statistics::Statistics()
    : initialized(false), cache_valid(false) {
}

Statistics::~Statistics() {
    if (initialized && cache_valid) {
        saveTodayCache();  // Save before closing
    }
    prefs.end();
}

bool Statistics::begin() {
    // Open NVS namespace in read-write mode
    if (!prefs.begin(NAMESPACE, false)) {
        Serial.println("[Statistics] ERROR: Failed to open NVS namespace");
        return false;
    }

    initialized = true;
    Serial.println("[Statistics] Initialized (90-day rolling window)");

    // Load today's data into cache
    loadTodayCache();

    return true;
}

void Statistics::recordWorkSession(uint16_t duration_min, bool completed) {
    if (!initialized) return;

    ensureTodayExists();

    if (completed) {
        today_cache.completed_sessions++;
        today_cache.work_minutes += duration_min;
    } else {
        today_cache.interruptions++;
    }

    cache_valid = true;
    saveTodayCache();

    Serial.printf("[Statistics] Work session: %u min, %s\n",
                  duration_min, completed ? "completed" : "interrupted");
}

void Statistics::recordBreakSession(uint16_t duration_min) {
    if (!initialized) return;

    ensureTodayExists();

    today_cache.break_minutes += duration_min;
    cache_valid = true;
    saveTodayCache();

    Serial.printf("[Statistics] Break session: %u min\n", duration_min);
}

void Statistics::recordInterruption() {
    if (!initialized) return;

    ensureTodayExists();

    today_cache.interruptions++;
    cache_valid = true;
    saveTodayCache();

    Serial.println("[Statistics] Interruption recorded");
}

Statistics::DayStats Statistics::getToday() const {
    return today_cache;
}

Statistics::DayStats Statistics::getDate(uint32_t epoch_days) const {
    if (!initialized) {
        return DayStats();
    }

    uint8_t index = getDayIndex(epoch_days);
    char key[16];
    snprintf(key, sizeof(key), "day_%u", index);

    DayStats stats;

    // Read from NVS (need mutable access to prefs)
    size_t len = sizeof(DayStats);
    if (!const_cast<Preferences&>(prefs).getBytes(key, &stats, len)) {
        // No data for this day
        return DayStats();
    }

    // Validate epoch_days matches
    if (stats.date_epoch_days != epoch_days) {
        // Data is for a different day (wraparound)
        return DayStats();
    }

    return stats;
}

void Statistics::getLast7Days(DayStats* out_array) const {
    if (!initialized || !out_array) return;

    uint32_t today_days = getTodayEpochDays();

    for (int i = 0; i < 7; i++) {
        uint32_t day = today_days - i;
        out_array[i] = getDate(day);
    }
}

void Statistics::getLast30Days(DayStats* out_array) const {
    if (!initialized || !out_array) return;

    uint32_t today_days = getTodayEpochDays();

    for (int i = 0; i < 30; i++) {
        uint32_t day = today_days - i;
        out_array[i] = getDate(day);
    }
}

uint16_t Statistics::getTotalCompleted() const {
    if (!initialized) return 0;

    // Sum all completed sessions across all days
    uint16_t total = 0;

    for (uint8_t i = 0; i < MAX_DAYS; i++) {
        char key[16];
        snprintf(key, sizeof(key), "day_%u", i);

        DayStats stats;
        size_t len = sizeof(DayStats);
        if (const_cast<Preferences&>(prefs).getBytes(key, &stats, len)) {
            total += stats.completed_sessions;
        }
    }

    return total;
}

uint16_t Statistics::getLast7DaysTotal() const {
    DayStats days[7];
    getLast7Days(days);

    uint16_t total = 0;
    for (int i = 0; i < 7; i++) {
        total += days[i].completed_sessions;
    }

    return total;
}

uint16_t Statistics::getLast30DaysTotal() const {
    DayStats days[30];
    getLast30Days(days);

    uint16_t total = 0;
    for (int i = 0; i < 30; i++) {
        total += days[i].completed_sessions;
    }

    return total;
}

float Statistics::getCompletionRate() const {
    if (!initialized) return 0.0f;

    uint16_t completed = 0;
    uint16_t interrupted = 0;

    // Sum last 30 days
    DayStats days[30];
    getLast30Days(days);

    for (int i = 0; i < 30; i++) {
        completed += days[i].completed_sessions;
        interrupted += days[i].interruptions;
    }

    uint16_t total = completed + interrupted;
    if (total == 0) return 0.0f;

    return (completed * 100.0f) / total;
}

void Statistics::cleanup() {
    if (!initialized) return;

    Serial.println("[Statistics] Cleaning up old data (>90 days)");

    uint32_t today_days = getTodayEpochDays();
    uint32_t cutoff_days = today_days - MAX_DAYS;

    // Iterate through all stored days and remove old ones
    for (uint8_t i = 0; i < MAX_DAYS; i++) {
        char key[16];
        snprintf(key, sizeof(key), "day_%u", i);

        DayStats stats;
        size_t len = sizeof(DayStats);
        if (prefs.getBytes(key, &stats, len)) {
            if (stats.date_epoch_days < cutoff_days) {
                prefs.remove(key);
                Serial.printf("[Statistics] Removed day %lu (too old)\n",
                              stats.date_epoch_days);
            }
        }
    }
}

void Statistics::clear() {
    if (!initialized) return;

    Serial.println("[Statistics] Clearing all statistics");
    prefs.clear();

    today_cache = DayStats();
    cache_valid = false;
}

// Private methods

uint32_t Statistics::getTodayEpochDays() const {
    // Get current time in seconds since Unix epoch
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    uint32_t epoch = tv.tv_sec;

    // Convert to days since epoch
    return epoch / 86400;
}

uint8_t Statistics::getDayIndex(uint32_t epoch_days) const {
    // Circular buffer: day index wraps around after 90 days
    return epoch_days % MAX_DAYS;
}

void Statistics::loadTodayCache() {
    uint32_t today_days = getTodayEpochDays();
    today_cache = getDate(today_days);
    cache_valid = true;
}

void Statistics::saveTodayCache() {
    if (!cache_valid) return;

    uint8_t index = getDayIndex(today_cache.date_epoch_days);
    char key[16];
    snprintf(key, sizeof(key), "day_%u", index);

    // Write to NVS
    size_t len = sizeof(DayStats);
    prefs.putBytes(key, &today_cache, len);
}

void Statistics::ensureTodayExists() {
    uint32_t today_days = getTodayEpochDays();

    if (cache_valid && today_cache.date_epoch_days == today_days) {
        // Cache is current
        return;
    }

    // New day or cache invalid
    Serial.printf("[Statistics] New day detected: %lu\n", today_days);

    today_cache.date_epoch_days = today_days;
    today_cache.completed_sessions = 0;
    today_cache.work_minutes = 0;
    today_cache.break_minutes = 0;
    today_cache.interruptions = 0;

    cache_valid = true;
    saveTodayCache();
}
