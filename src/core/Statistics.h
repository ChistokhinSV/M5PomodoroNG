#ifndef STATISTICS_H
#define STATISTICS_H

#include <Preferences.h>
#include <cstdint>
#include <ctime>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/**
 * Statistics tracking and storage using ESP32 NVS
 *
 * Tracks 90-day rolling window of:
 * - Completed Pomodoro sessions per day
 * - Total work time per day
 * - Break time per day
 * - Session completion rate
 *
 * Storage strategy:
 * - Circular buffer: 90 days × 12 bytes = 1080 bytes
 * - Day index = epoch_days % 90
 * - Auto-cleanup of old data
 *
 * Thread-Safety (MP-47):
 * - All public methods protected by g_stats_mutex (global mutex)
 * - NVS operations can be slow (100ms), uses 100ms timeout
 * - Safe to call from any task (Core 0 or Core 1)
 * - Uses RAII MutexGuard for automatic mutex release
 */
class Statistics {
public:
    struct DayStats {
        uint32_t date_epoch_days;    // Days since Unix epoch (for validation)
        uint16_t completed_sessions;  // Pomodoros completed
        uint16_t work_minutes;        // Total work time
        uint16_t break_minutes;       // Total break time
        uint8_t interruptions;        // Times paused/stopped mid-session
    };

    Statistics();
    ~Statistics();

    // Initialize NVS storage
    bool begin();

    // Record session completion
    void recordWorkSession(uint16_t duration_min, bool completed);
    void recordBreakSession(uint16_t duration_min);
    void recordInterruption();

    // Query statistics
    DayStats getToday() const;
    DayStats getDate(uint32_t epoch_days) const;
    void getLast7Days(DayStats* out_array) const;
    void getLast30Days(DayStats* out_array) const;

    // Aggregated stats
    uint32_t getTotalCompleted() const;        // Lifetime total (buffer + overflow)
    uint16_t getLast7DaysTotal() const;
    uint16_t getLast30DaysTotal() const;
    float getCompletionRate() const;           // % of sessions completed vs interrupted

    // Maintenance
    void cleanup();                            // Remove data older than 90 days
    void clear();                              // Clear all statistics

private:
    Preferences prefs;
    bool initialized = false;

    static constexpr const char* NAMESPACE = "stats";
    static constexpr uint8_t MAX_DAYS = 90;
    static constexpr const char* KEY_OVERFLOW = "overflow";

    // Current day cache (to avoid repeated NVS writes)
    DayStats today_cache;
    bool cache_valid = false;

    // Sessions that fell out of the 90-day buffer. Bumped whenever a slot is
    // overwritten with new-day data. Persisted to KEY_OVERFLOW. Added on top
    // of the buffer sum in getTotalCompleted() to give a true lifetime count.
    uint32_t lifetime_overflow = 0;

    // Helper methods
    uint32_t getTodayEpochDays() const;
    uint8_t getDayIndex(uint32_t epoch_days) const;
    void loadTodayCache();
    void saveTodayCache();
    void ensureTodayExists();
    // Read the raw slot regardless of which day it currently belongs to. Used
    // to harvest a slot's count into overflow before overwriting it.
    bool readRawSlot(uint8_t index, DayStats& out) const;
};

#endif // STATISTICS_H