#ifndef CONFIG_H
#define CONFIG_H

#include <Preferences.h>
#include <cstdint>

/**
 * Configuration management using ESP32 NVS (Non-Volatile Storage)
 *
 * Handles persistent storage of:
 * - Pomodoro settings (durations, modes)
 * - UI preferences (brightness, sound, haptics)
 * - Network credentials (WiFi, AWS IoT)
 * - Power management settings
 */
class Config {
public:
    // Pomodoro timing settings
    struct PomodoroSettings {
        uint16_t work_duration_min = 25;       // Classic: 25min
        uint16_t short_break_min = 5;          // Classic: 5min
        uint16_t long_break_min = 15;          // Classic: 15min
        uint8_t sessions_before_long = 4;      // Classic: 4 sessions
        bool auto_start_breaks = true;
        bool auto_start_work = false;
    };

    // UI preferences
    struct UISettings {
        uint8_t brightness = 80;               // 0-100%
        bool sound_enabled = true;
        uint8_t sound_volume = 70;             // 0-100%
        bool haptic_enabled = true;
        bool show_seconds = true;
        uint8_t screen_timeout_sec = 30;       // 0 = never
    };

    // Network settings
    struct NetworkSettings {
        char wifi_ssid[32] = "";
        char wifi_password[64] = "";
        char mqtt_broker[128] = "";
        uint16_t mqtt_port = 8883;
        char mqtt_client_id[32] = "";
        bool cloud_sync_enabled = false;
        uint16_t sync_interval_min = 5;
    };

    // Power management
    struct PowerSettings {
        bool auto_sleep_enabled = true;
        uint16_t sleep_after_min = 60;         // Minutes of inactivity
        bool wake_on_rotation = true;
        uint8_t min_battery_percent = 10;      // Low battery warning
    };

    Config();
    ~Config();

    // Initialize NVS and load configuration
    bool begin();

    // Load/Save methods
    bool load();
    bool save();
    void reset();  // Reset to defaults

    // Getters
    const PomodoroSettings& getPomodoro() const { return pomodoro; }
    const UISettings& getUI() const { return ui; }
    const NetworkSettings& getNetwork() const { return network; }
    const PowerSettings& getPower() const { return power; }

    // Setters (mark dirty for lazy save)
    void setPomodoro(const PomodoroSettings& settings);
    void setUI(const UISettings& settings);
    void setNetwork(const NetworkSettings& settings);
    void setPower(const PowerSettings& settings);

    // Dirty flag management
    bool isDirty() const { return dirty; }
    void markDirty() { dirty = true; }

private:
    Preferences prefs;
    bool initialized = false;
    bool dirty = false;

    PomodoroSettings pomodoro;
    UISettings ui;
    NetworkSettings network;
    PowerSettings power;

    static constexpr const char* NAMESPACE = "config";
};

#endif // CONFIG_H
