#include "TimeManager.h"
#include <Arduino.h>
#include <time.h>

TimeManager::TimeManager()
    : ntp_client(nullptr),
      time_synced(false),
      last_sync_epoch(0),
      last_sync_millis(0),
      last_midnight_epoch(0),
      utc_offset_sec(0),
      time_source(TimeSource::UNKNOWN),
      drift_ppm(0.0f),
      drift_samples(0) {
}

TimeManager::~TimeManager() {
    if (ntp_client) {
        delete ntp_client;
    }
}

bool TimeManager::begin(int32_t utc_offset) {
    utc_offset_sec = utc_offset;

    // Initialize NTPClient with UDP (for future NTP sync)
    ntp_client = new NTPClient(udp, NTP_SERVER, utc_offset_sec, SYNC_INTERVAL_MS);

    if (!ntp_client) {
        Serial.println("[TimeManager] ERROR: Failed to create NTP client");
        return false;
    }

    Serial.printf("[TimeManager] Initialized (UTC offset: %ld seconds)\n", utc_offset_sec);

    // RTC-first strategy: Try to load time from M5.Rtc
    if (loadTimeFromRTC()) {
        Serial.println("[TimeManager] Time loaded from M5.Rtc (BM8563)");
        time_source = TimeSource::RTC;
        calculateMidnight();
        return true;
    }

    // RTC invalid - set default time and save to RTC
    Serial.println("[TimeManager] WARN: RTC time invalid, using default (2025-01-01)");
    if (saveTimeToRTC(DEFAULT_EPOCH)) {
        last_sync_epoch = DEFAULT_EPOCH;
        last_sync_millis = millis();
        time_source = TimeSource::FALLBACK_DEFAULT;
        calculateMidnight();
        return true;
    }

    Serial.println("[TimeManager] ERROR: Failed to initialize time");
    return false;
}

bool TimeManager::syncNow() {
    if (!ntp_client) {
        Serial.println("[TimeManager] ERROR: NTP client not initialized");
        return false;
    }

    Serial.println("[TimeManager] Syncing time from NTP...");

    // Attempt NTP sync (non-blocking update)
    if (!ntp_client->update()) {
        Serial.println("[TimeManager] ERROR: NTP sync failed");
        return false;
    }

    // Get epoch time from NTP
    uint32_t ntp_epoch = ntp_client->getEpochTime();

    if (ntp_epoch == 0) {
        Serial.println("[TimeManager] ERROR: Invalid NTP time");
        return false;
    }

    // Update drift estimate if we have a previous sync
    if (time_synced && last_sync_epoch > 0) {
        // Compare NTP time with RTC to calculate drift
        uint32_t rtc_epoch = rtcToEpoch();
        if (rtc_epoch > 0) {
            int32_t drift_sec = ntp_epoch - rtc_epoch;
            if (abs(drift_sec) > 2) {  // Drift > 2 seconds
                Serial.printf("[TimeManager] RTC drift detected: %ld seconds\n", drift_sec);
            }
        }
        updateDriftEstimate(ntp_epoch);
    }

    // Save NTP time to M5.Rtc (RTC becomes NTP-synced)
    if (!saveTimeToRTC(ntp_epoch)) {
        Serial.println("[TimeManager] ERROR: Failed to save NTP time to RTC");
        return false;
    }

    // Update sync tracking
    last_sync_epoch = ntp_epoch;
    last_sync_millis = millis();
    time_synced = true;
    time_source = TimeSource::NTP;

    // Calculate midnight boundary
    calculateMidnight();

    Serial.printf("[TimeManager] NTP synced: %lu (saved to RTC)\n", ntp_epoch);
    return true;
}

uint32_t TimeManager::getEpoch() const {
    // Always read from M5.Rtc (primary source)
    return rtcToEpoch();
}

uint32_t TimeManager::getEpochDays() const {
    uint32_t epoch = getEpoch();
    if (epoch == 0) return 0;

    // Days since Unix epoch (86400 seconds per day)
    return epoch / 86400;
}

void TimeManager::getLocalTime(struct tm& timeinfo) const {
    uint32_t epoch = getEpoch();
    time_t raw_time = epoch;
    localtime_r(&raw_time, &timeinfo);
}

void TimeManager::getTimeString(char* buffer, size_t len) const {
    struct tm timeinfo;
    getLocalTime(timeinfo);

    snprintf(buffer, len, "%02d:%02d:%02d",
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}

void TimeManager::getDateString(char* buffer, size_t len) const {
    struct tm timeinfo;
    getLocalTime(timeinfo);

    snprintf(buffer, len, "%04d-%02d-%02d",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
}

bool TimeManager::isMidnightCrossed() {
    uint32_t current_epoch = getEpoch();
    if (current_epoch == 0) return false;

    // Calculate current day start (midnight)
    uint32_t current_day_start = (current_epoch / 86400) * 86400;

    if (last_midnight_epoch == 0) {
        // First check, initialize
        last_midnight_epoch = current_day_start;
        return false;
    }

    if (current_day_start > last_midnight_epoch) {
        // Midnight crossed!
        last_midnight_epoch = current_day_start;
        Serial.println("[TimeManager] Midnight crossed");
        return true;
    }

    return false;
}

uint32_t TimeManager::getSecondsSinceMidnight() const {
    uint32_t current_epoch = getEpoch();
    if (current_epoch == 0) return 0;

    // Current day start (midnight)
    uint32_t day_start = (current_epoch / 86400) * 86400;

    return current_epoch - day_start;
}

void TimeManager::update() {
    if (!time_synced) {
        return;  // Nothing to update
    }

    // Check if we should re-sync
    uint32_t elapsed = millis() - last_sync_millis;
    if (elapsed >= SYNC_INTERVAL_MS) {
        syncNow();  // Attempt re-sync (non-blocking)
    }
}

// Private methods

bool TimeManager::loadTimeFromRTC() {
    if (!isRTCValid()) {
        return false;
    }

    uint32_t rtc_epoch = rtcToEpoch();
    if (rtc_epoch == 0) {
        return false;
    }

    last_sync_epoch = rtc_epoch;
    last_sync_millis = millis();

    Serial.printf("[TimeManager] RTC time: %lu (Unix epoch)\n", rtc_epoch);
    return true;
}

bool TimeManager::saveTimeToRTC(uint32_t epoch) {
    m5::rtc_datetime_t dt;
    epochToRTC(epoch, dt);

    // Set time on M5.Rtc (BM8563)
    M5.Rtc.setDateTime(dt);

    Serial.printf("[TimeManager] Saved to RTC: %04d-%02d-%02d %02d:%02d:%02d\n",
                  dt.date.year, dt.date.month, dt.date.date,
                  dt.time.hours, dt.time.minutes, dt.time.seconds);
    return true;
}

bool TimeManager::isRTCValid() const {
    auto dt = M5.Rtc.getDateTime();

    // Check if year is reasonable (>= 2020)
    if (dt.date.year < MIN_VALID_YEAR) {
        Serial.printf("[TimeManager] RTC year invalid: %d (< %d)\n",
                      dt.date.year, MIN_VALID_YEAR);
        return false;
    }

    // Check month and day ranges
    if (dt.date.month < 1 || dt.date.month > 12) {
        Serial.printf("[TimeManager] RTC month invalid: %d\n", dt.date.month);
        return false;
    }

    if (dt.date.date < 1 || dt.date.date > 31) {
        Serial.printf("[TimeManager] RTC day invalid: %d\n", dt.date.date);
        return false;
    }

    return true;
}

uint32_t TimeManager::rtcToEpoch() const {
    auto dt = M5.Rtc.getDateTime();

    // Convert to struct tm
    struct tm timeinfo = {};
    timeinfo.tm_year = dt.date.year - 1900;   // Years since 1900
    timeinfo.tm_mon = dt.date.month - 1;       // 0-11
    timeinfo.tm_mday = dt.date.date;           // 1-31
    timeinfo.tm_hour = dt.time.hours;          // 0-23
    timeinfo.tm_min = dt.time.minutes;         // 0-59
    timeinfo.tm_sec = dt.time.seconds;         // 0-59
    timeinfo.tm_isdst = -1;                    // Let mktime determine DST

    // Convert to Unix epoch (apply UTC offset)
    time_t epoch = mktime(&timeinfo);
    if (epoch == -1) {
        Serial.println("[TimeManager] ERROR: Failed to convert RTC to epoch");
        return 0;
    }

    // mktime assumes local time, so subtract UTC offset to get true UTC epoch
    epoch -= utc_offset_sec;

    return (uint32_t)epoch;
}

void TimeManager::epochToRTC(uint32_t epoch, m5::rtc_datetime_t& dt) const {
    // Add UTC offset to get local time
    time_t local_epoch = epoch + utc_offset_sec;

    // Convert epoch to struct tm
    struct tm timeinfo;
    gmtime_r(&local_epoch, &timeinfo);

    // Fill RTC datetime struct
    dt.date.year = timeinfo.tm_year + 1900;
    dt.date.month = timeinfo.tm_mon + 1;
    dt.date.date = timeinfo.tm_mday;
    dt.date.weekDay = timeinfo.tm_wday;

    dt.time.hours = timeinfo.tm_hour;
    dt.time.minutes = timeinfo.tm_min;
    dt.time.seconds = timeinfo.tm_sec;
}

void TimeManager::updateDriftEstimate(uint32_t ntp_epoch) {
    // Calculate expected time based on last sync + elapsed
    uint32_t elapsed_ms = millis() - last_sync_millis;
    uint32_t elapsed_sec = elapsed_ms / 1000;
    uint32_t expected_epoch = last_sync_epoch + elapsed_sec;

    // Calculate drift (difference between expected and actual)
    int32_t drift_sec = ntp_epoch - expected_epoch;

    if (abs(drift_sec) > 10) {
        // Drift too large, might be a time jump or first sync
        Serial.printf("[TimeManager] Large drift detected: %ld seconds\n", drift_sec);
        drift_ppm = 0.0f;  // Reset drift estimate
        drift_samples = 0;
        return;
    }

    // Calculate drift in parts per million (PPM)
    float measured_drift_ppm = (drift_sec * 1000000.0f) / elapsed_sec;

    // Update running average of drift
    if (drift_samples == 0) {
        drift_ppm = measured_drift_ppm;
    } else {
        // Weighted average (favor recent measurements)
        drift_ppm = (drift_ppm * 0.8f) + (measured_drift_ppm * 0.2f);
    }

    drift_samples++;

    Serial.printf("[TimeManager] Drift: %.2f PPM (after %lu samples)\n",
                  drift_ppm, drift_samples);
}

void TimeManager::calculateMidnight() {
    uint32_t epoch = last_sync_epoch;

    // Calculate start of current day (midnight UTC + offset)
    uint32_t day_start = (epoch / 86400) * 86400;
    last_midnight_epoch = day_start;
}
