#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <WiFiUdp.h>
#include <NTPClient.h>
#include <cstdint>
#include <ctime>

/**
 * Time management with NTP synchronization and RTC drift compensation
 *
 * Features:
 * - NTP sync from pool.ntp.org (or custom server)
 * - ESP32 internal RTC tracking
 * - Drift learning and compensation
 * - Timezone support (UTC offset)
 * - Automatic midnight detection for daily stats reset
 *
 * Sync strategy:
 * - Initial sync on WiFi connect
 * - Re-sync every 6 hours when WiFi available
 * - Fall back to RTC + drift compensation when offline
 */
class TimeManager {
public:
    TimeManager();
    ~TimeManager();

    // Initialize with timezone offset (seconds from UTC)
    bool begin(int32_t utc_offset_sec = 0);

    // NTP synchronization
    bool syncNow();                    // Force immediate sync
    bool isTimeSynced() const { return time_synced; }
    uint32_t getLastSyncEpoch() const { return last_sync_epoch; }

    // Time getters
    uint32_t getEpoch() const;         // Current Unix timestamp
    uint32_t getEpochDays() const;     // Days since Unix epoch
    void getLocalTime(struct tm& timeinfo) const;

    // Formatted time strings
    void getTimeString(char* buffer, size_t len) const;  // HH:MM:SS
    void getDateString(char* buffer, size_t len) const;  // YYYY-MM-DD

    // Midnight detection
    bool isMidnightCrossed();          // Check if we crossed midnight since last call
    uint32_t getSecondsSinceMidnight() const;

    // Timezone management
    void setUTCOffset(int32_t seconds) { utc_offset_sec = seconds; }
    int32_t getUTCOffset() const { return utc_offset_sec; }

    // Update loop (call periodically)
    void update();

    // RTC drift compensation
    float getDriftPPM() const { return drift_ppm; }  // Parts per million

private:
    WiFiUDP udp;
    NTPClient* ntp_client = nullptr;

    bool time_synced = false;
    uint32_t last_sync_epoch = 0;
    uint32_t last_sync_millis = 0;
    uint32_t last_midnight_epoch = 0;
    int32_t utc_offset_sec = 0;

    // Drift tracking
    float drift_ppm = 0.0f;            // Learned drift in parts per million
    uint32_t drift_samples = 0;

    // NTP config
    static constexpr const char* NTP_SERVER = "pool.ntp.org";
    static constexpr uint32_t SYNC_INTERVAL_MS = 6 * 60 * 60 * 1000;  // 6 hours

    // Helper methods
    uint32_t getCompensatedEpoch() const;
    void updateDriftEstimate(uint32_t ntp_epoch);
    void calculateMidnight();
};

#endif // TIME_MANAGER_H