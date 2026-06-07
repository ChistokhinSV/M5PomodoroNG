#include "Statistics.h"
#include "SyncPrimitives.h"
#include "TimeManager.h"
#include "../utils/MutexGuard.h"
#include <Arduino.h>
#include <sys/time.h>

// Day bucketing is driven by the global TimeManager when it's available.
// Until then (very early boot, before main.cpp wires it up) we fall back to
// the system clock, which is OK because Statistics::begin() runs late enough
// that the pointer is already set in normal operation.
extern TimeManager* g_timeManager;

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

    // Lifetime overflow — sessions that have aged out of the 90-day buffer.
    lifetime_overflow = prefs.getUInt(KEY_OVERFLOW, 0);
    Serial.printf("[Statistics] Initialized (90-day rolling window, overflow=%lu)\n",
                  (unsigned long)lifetime_overflow);

    // Load today's data into cache
    loadTodayCache();

    return true;
}

void Statistics::recordWorkSession(uint16_t duration_min, bool completed) {
    MutexGuard guard(g_stats_mutex, "g_stats_mutex", 100);
    if (!guard.isLocked()) {
        Serial.println("[Statistics] ERROR: Failed to acquire mutex in recordWorkSession");
        return;
    }

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
    MutexGuard guard(g_stats_mutex, "g_stats_mutex", 100);
    if (!guard.isLocked()) {
        Serial.println("[Statistics] ERROR: Failed to acquire mutex in recordBreakSession");
        return;
    }

    if (!initialized) return;

    ensureTodayExists();

    today_cache.break_minutes += duration_min;
    cache_valid = true;
    saveTodayCache();

    Serial.printf("[Statistics] Break session: %u min\n", duration_min);
}

void Statistics::recordInterruption() {
    MutexGuard guard(g_stats_mutex, "g_stats_mutex", 100);
    if (!guard.isLocked()) {
        Serial.println("[Statistics] ERROR: Failed to acquire mutex in recordInterruption");
        return;
    }

    if (!initialized) return;

    ensureTodayExists();

    today_cache.interruptions++;
    cache_valid = true;
    saveTodayCache();

    Serial.println("[Statistics] Interruption recorded");
}

Statistics::DayStats Statistics::getToday() const {
    MutexGuard guard(g_stats_mutex, "g_stats_mutex", 100);
    if (!guard.isLocked()) {
        Serial.println("[Statistics] ERROR: Failed to acquire mutex in getToday");
        return DayStats{};  // Return empty stats on error
    }

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

    // Check if key exists first (avoid NVS "NOT_FOUND" errors)
    if (!const_cast<Preferences&>(prefs).isKey(key)) {
        // No data for this day
        return DayStats();
    }

    // Read from NVS (need mutable access to prefs)
    size_t len = sizeof(DayStats);
    if (!const_cast<Preferences&>(prefs).getBytes(key, &stats, len)) {
        // Failed to read data
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

uint32_t Statistics::getTotalCompleted() const {
    if (!initialized) return 0;

    // Sum all completed sessions across the 90-day buffer, plus the overflow
    // counter that absorbed counts from slots before they were overwritten.
    uint32_t total = lifetime_overflow;

    for (uint8_t i = 0; i < MAX_DAYS; i++) {
        char key[16];
        snprintf(key, sizeof(key), "day_%u", i);

        if (!const_cast<Preferences&>(prefs).isKey(key)) {
            continue;
        }

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

    // Iterate through all stored days and remove old ones, folding their
    // counts into overflow first so getTotalCompleted() doesn't drop them.
    for (uint8_t i = 0; i < MAX_DAYS; i++) {
        char key[16];
        snprintf(key, sizeof(key), "day_%u", i);

        DayStats stats;
        size_t len = sizeof(DayStats);
        if (prefs.getBytes(key, &stats, len)) {
            if (stats.date_epoch_days < cutoff_days) {
                lifetime_overflow += stats.completed_sessions;
                prefs.remove(key);
                Serial.printf("[Statistics] Removed day %lu (too old, +%u into overflow)\n",
                              (unsigned long)stats.date_epoch_days,
                              stats.completed_sessions);
            }
        }
    }
    prefs.putUInt(KEY_OVERFLOW, lifetime_overflow);
}

void Statistics::clear() {
    if (!initialized) return;

    Serial.println("[Statistics] Clearing all statistics");
    prefs.clear();

    today_cache = DayStats();
    cache_valid = false;
    lifetime_overflow = 0;  // prefs.clear() already wiped the NVS key
}

// Private methods

uint32_t Statistics::getTodayEpochDays() const {
    // Prefer TimeManager: it computes days from the RTC-derived UTC epoch
    // shifted by the local TZ offset, so the day boundary lands at *local*
    // midnight. Without this, a session at 23:55 local (UTC+3) would be
    // counted under tomorrow's UTC day — and at boot, today_cache would
    // load yesterday's slot when the system clock hadn't drifted into the
    // new UTC day yet.
    if (g_timeManager) {
        uint32_t days = g_timeManager->getLocalEpochDays();
        if (days != 0) return days;
    }

    // Fallback for very-early-boot reads (before main wires g_timeManager).
    // Returns UTC days; the cache will get re-validated by ensureTodayExists
    // on the first recordWorkSession() once TimeManager is up.
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return static_cast<uint32_t>(tv.tv_sec / 86400);
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

    // Before overwriting the slot at today's index, harvest whatever count
    // is currently sitting there into the lifetime overflow. The slot at
    // today_days % 90 can only belong to today, day-90, day-180, ... so any
    // mismatch means we're about to evict a historical day.
    DayStats prev;
    if (readRawSlot(getDayIndex(today_days), prev)) {
        if (prev.date_epoch_days != 0 && prev.date_epoch_days != today_days) {
            uint32_t before = lifetime_overflow;
            lifetime_overflow += prev.completed_sessions;
            prefs.putUInt(KEY_OVERFLOW, lifetime_overflow);
            Serial.printf("[Statistics] Day %lu rolled into overflow (+%u sessions, overflow %lu -> %lu)\n",
                          (unsigned long)prev.date_epoch_days,
                          prev.completed_sessions,
                          (unsigned long)before,
                          (unsigned long)lifetime_overflow);
        }
    }

    today_cache.date_epoch_days = today_days;
    today_cache.completed_sessions = 0;
    today_cache.work_minutes = 0;
    today_cache.break_minutes = 0;
    today_cache.interruptions = 0;

    cache_valid = true;
    saveTodayCache();
}

bool Statistics::readRawSlot(uint8_t index, DayStats& out) const {
    char key[16];
    snprintf(key, sizeof(key), "day_%u", index);

    if (!const_cast<Preferences&>(prefs).isKey(key)) {
        return false;
    }

    size_t len = sizeof(DayStats);
    if (!const_cast<Preferences&>(prefs).getBytes(key, &out, len)) {
        return false;
    }
    return true;
}
