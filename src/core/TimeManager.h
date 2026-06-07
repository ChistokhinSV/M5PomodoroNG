#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <M5Unified.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <cstdint>
#include <ctime>

// Forward declaration
class SDManager;

/**
 * Time management with M5.Rtc (BM8563) as primary source + NTP sync
 *
 * Features:
 * - M5.Rtc (BM8563) with backup battery as PRIMARY time source
 * - NTP sync from pool.ntp.org (updates RTC when WiFi available)
 * - Drift learning and compensation
 * - Timezone support (UTC offset)
 * - Automatic midnight detection for daily stats reset
 * - SD card emergency fallback (MP-76)
 *
 * Time Source Priority (RTC-first strategy):
 * 1. M5.Rtc (BM8563) - persists time across power cycles (backup battery)
 * 2. NTP sync - updates RTC every 6 hours when WiFi available
 * 3. SD card file - emergency fallback if RTC invalid (year < 2020)
 * 4. Default time - 2025-01-01 00:00:00 (last resort)
 *
 * Why RTC-first?
 * - BM8563 RTC has coin cell backup battery (persists hours to years)
 * - More reliable than SD card (removable, corruption risk)
 * - Faster than NTP (no WiFi dependency)
 * - NTP only used to correct RTC drift, not as primary source
 */
class TimeManager {
public:
    TimeManager();
    ~TimeManager();

    // Initialize with a POSIX TZ string (e.g. "CET-1CEST,M3.5.0,M10.5.0/3" or
    // "UTC0"). Sets the process TZ env so localtime_r / mktime do the right
    // thing — including automatic DST when the rules are encoded. Loads time
    // from M5.Rtc, validates, falls back to SD/default if invalid.
    bool begin(const char* tz_spec, SDManager* sd = nullptr);
    // Legacy overload: kept for callers that still pass a raw seconds offset.
    // Synthesizes a POSIX TZ string with no DST and forwards.
    bool begin(int32_t utc_offset_sec = 0, SDManager* sd = nullptr);

    // NTP synchronization (updates M5.Rtc)
    bool syncNow();                    // Force immediate NTP sync, update RTC
    bool isTimeSynced() const { return time_synced; }
    uint32_t getLastSyncEpoch() const { return last_sync_epoch; }

    // Time getters (from M5.Rtc)
    uint32_t getEpoch() const;         // Current Unix timestamp from RTC
    uint32_t getEpochDays() const;     // Days since Unix epoch (UTC midnight)
    // Days since Unix epoch counted by the *local* calendar — i.e. the
    // boundary is local midnight (after DST/UTC offset). This is the field
    // Statistics uses to bucket per-day counts so a session at 23:55 local
    // doesn't land in tomorrow's slot just because UTC has already rolled.
    uint32_t getLocalEpochDays() const;
    void getLocalTime(struct tm& timeinfo) const;

    // Time source status
    enum class TimeSource {
        UNKNOWN,          // No valid time available
        RTC,              // M5.Rtc (BM8563)
        NTP,              // NTP synced (also updates RTC)
        SD_FILE,          // SD card file fallback
        FALLBACK_DEFAULT  // Hardcoded default (2025-01-01)
    };
    TimeSource getTimeSource() const { return time_source; }

    // Formatted time strings
    void getTimeString(char* buffer, size_t len) const;  // HH:MM:SS
    void getDateString(char* buffer, size_t len) const;  // YYYY-MM-DD

    // Midnight detection
    bool isMidnightCrossed();          // Check if we crossed midnight since last call
    uint32_t getSecondsSinceMidnight() const;
    uint8_t getCurrentWeekday() const; // 0=Mon, 1=Tue, ..., 6=Sun

    // Timezone management
    void setUTCOffset(int32_t seconds) { utc_offset_sec = seconds; }
    int32_t getUTCOffset() const { return utc_offset_sec; }

    // Update loop (call periodically)
    void update();

    // SD card persistence (emergency fallback only)
    void saveTimeToSD(SDManager& sd);  // Save time to /config/lasttime.txt

    // RTC drift compensation
    float getDriftPPM() const { return drift_ppm; }  // Parts per million

private:
    WiFiUDP udp;
    NTPClient* ntp_client = nullptr;

    bool time_synced = false;          // NTP synced at least once
    uint32_t last_sync_epoch = 0;      // Last NTP sync time
    uint32_t last_sync_millis = 0;     // millis() at last sync
    uint32_t last_midnight_epoch = 0;  // Last midnight boundary
    int32_t utc_offset_sec = 0;        // Timezone offset

    TimeSource time_source = TimeSource::UNKNOWN;

    // Drift tracking (for RTC accuracy monitoring)
    float drift_ppm = 0.0f;            // Learned drift in parts per million
    uint32_t drift_samples = 0;

    // NTP config
    static constexpr const char* NTP_SERVER = "pool.ntp.org";
    static constexpr uint32_t SYNC_INTERVAL_MS = 6 * 60 * 60 * 1000;  // 6 hours

    // RTC validation
    static constexpr uint32_t MIN_VALID_YEAR = 2020;   // RTC year < 2020 = invalid
    static constexpr uint32_t DEFAULT_EPOCH = 1735689600; // 2025-01-01 00:00:00 UTC

    // Helper methods
    bool loadTimeFromRTC();                     // Load time from M5.Rtc
    bool saveTimeToRTC(uint32_t epoch);         // Save time to M5.Rtc
    bool isRTCValid() const;                    // Check if RTC has valid time
    uint32_t rtcToEpoch() const;                // Convert M5.Rtc to Unix epoch
    void epochToRTC(uint32_t epoch, m5::rtc_datetime_t& dt) const;  // Convert epoch to RTC struct
    void updateDriftEstimate(uint32_t ntp_epoch);
    void calculateMidnight();
    bool loadTimeFromSD(SDManager& sd);            // Load emergency time from SD file
};

#endif // TIME_MANAGER_H