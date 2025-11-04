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

    // Initialize NTPClient with UDP
    ntp_client = new NTPClient(udp, NTP_SERVER, utc_offset_sec, SYNC_INTERVAL_MS);

    if (!ntp_client) {
        Serial.println("[TimeManager] ERROR: Failed to create NTP client");
        return false;
    }

    Serial.printf("[TimeManager] Initialized (UTC offset: %ld seconds)\n", utc_offset_sec);
    return true;
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
        updateDriftEstimate(ntp_epoch);
    }

    // Update sync tracking
    last_sync_epoch = ntp_epoch;
    last_sync_millis = millis();
    time_synced = true;

    // Calculate midnight boundary
    calculateMidnight();

    Serial.printf("[TimeManager] Synced successfully: %lu (Unix epoch)\n", ntp_epoch);
    return true;
}

uint32_t TimeManager::getEpoch() const {
    if (!time_synced) {
        return 0;  // No time available
    }

    return getCompensatedEpoch();
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

uint32_t TimeManager::getCompensatedEpoch() const {
    // Calculate elapsed time since last sync
    uint32_t elapsed_ms = millis() - last_sync_millis;
    uint32_t elapsed_sec = elapsed_ms / 1000;

    // Apply drift compensation
    float drift_correction = (elapsed_sec * drift_ppm) / 1000000.0f;

    return last_sync_epoch + elapsed_sec + (int32_t)drift_correction;
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
