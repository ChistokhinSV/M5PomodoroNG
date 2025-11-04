#include "Config.h"
#include <Arduino.h>

Config::Config()
    : initialized(false), dirty(false) {
}

Config::~Config() {
    if (initialized && dirty) {
        save();  // Auto-save on destruction if dirty
    }
    prefs.end();
}

bool Config::begin() {
    // Open NVS namespace in read-write mode
    if (!prefs.begin(NAMESPACE, false)) {
        Serial.println("[Config] ERROR: Failed to open NVS namespace");
        return false;
    }

    initialized = true;

    // Load existing configuration or set defaults
    if (!load()) {
        Serial.println("[Config] No saved config, using defaults");
        save();  // Save defaults
    }

    Serial.println("[Config] Initialized");
    return true;
}

bool Config::load() {
    if (!initialized) {
        Serial.println("[Config] ERROR: Not initialized");
        return false;
    }

    // Check if config exists (look for version key)
    uint8_t version = prefs.getUChar("version", 0);
    if (version == 0) {
        // No saved configuration
        return false;
    }

    Serial.printf("[Config] Loading configuration (version %d)\n", version);

    // Load Pomodoro settings
    pomodoro.work_duration_min = prefs.getUShort("pom_work", 25);
    pomodoro.short_break_min = prefs.getUShort("pom_short", 5);
    pomodoro.long_break_min = prefs.getUShort("pom_long", 15);
    pomodoro.sessions_before_long = prefs.getUChar("pom_sessions", 4);
    pomodoro.auto_start_breaks = prefs.getBool("pom_auto_brk", true);
    pomodoro.auto_start_work = prefs.getBool("pom_auto_wrk", false);

    // Load UI settings
    ui.brightness = prefs.getUChar("ui_bright", 80);
    ui.sound_enabled = prefs.getBool("ui_sound", true);
    ui.sound_volume = prefs.getUChar("ui_volume", 70);
    ui.haptic_enabled = prefs.getBool("ui_haptic", true);
    ui.show_seconds = prefs.getBool("ui_seconds", true);
    ui.screen_timeout_sec = prefs.getUChar("ui_timeout", 30);

    // Load Network settings
    prefs.getString("net_ssid", network.wifi_ssid, sizeof(network.wifi_ssid));
    prefs.getString("net_pass", network.wifi_password, sizeof(network.wifi_password));
    prefs.getString("net_broker", network.mqtt_broker, sizeof(network.mqtt_broker));
    network.mqtt_port = prefs.getUShort("net_port", 8883);
    prefs.getString("net_client", network.mqtt_client_id, sizeof(network.mqtt_client_id));
    network.cloud_sync_enabled = prefs.getBool("net_sync", false);
    network.sync_interval_min = prefs.getUShort("net_interval", 5);

    // Load Power settings
    power.auto_sleep_enabled = prefs.getBool("pwr_auto", true);
    power.sleep_after_min = prefs.getUShort("pwr_timeout", 60);
    power.wake_on_rotation = prefs.getBool("pwr_wake_rot", true);
    power.min_battery_percent = prefs.getUChar("pwr_min_bat", 10);

    dirty = false;
    Serial.println("[Config] Loaded successfully");
    return true;
}

bool Config::save() {
    if (!initialized) {
        Serial.println("[Config] ERROR: Not initialized");
        return false;
    }

    Serial.println("[Config] Saving configuration...");

    // Save version (for future migrations)
    prefs.putUChar("version", 1);

    // Save Pomodoro settings
    prefs.putUShort("pom_work", pomodoro.work_duration_min);
    prefs.putUShort("pom_short", pomodoro.short_break_min);
    prefs.putUShort("pom_long", pomodoro.long_break_min);
    prefs.putUChar("pom_sessions", pomodoro.sessions_before_long);
    prefs.putBool("pom_auto_brk", pomodoro.auto_start_breaks);
    prefs.putBool("pom_auto_wrk", pomodoro.auto_start_work);

    // Save UI settings
    prefs.putUChar("ui_bright", ui.brightness);
    prefs.putBool("ui_sound", ui.sound_enabled);
    prefs.putUChar("ui_volume", ui.sound_volume);
    prefs.putBool("ui_haptic", ui.haptic_enabled);
    prefs.putBool("ui_seconds", ui.show_seconds);
    prefs.putUChar("ui_timeout", ui.screen_timeout_sec);

    // Save Network settings
    prefs.putString("net_ssid", network.wifi_ssid);
    prefs.putString("net_pass", network.wifi_password);
    prefs.putString("net_broker", network.mqtt_broker);
    prefs.putUShort("net_port", network.mqtt_port);
    prefs.putString("net_client", network.mqtt_client_id);
    prefs.putBool("net_sync", network.cloud_sync_enabled);
    prefs.putUShort("net_interval", network.sync_interval_min);

    // Save Power settings
    prefs.putBool("pwr_auto", power.auto_sleep_enabled);
    prefs.putUShort("pwr_timeout", power.sleep_after_min);
    prefs.putBool("pwr_wake_rot", power.wake_on_rotation);
    prefs.putUChar("pwr_min_bat", power.min_battery_percent);

    dirty = false;
    Serial.println("[Config] Saved successfully");
    return true;
}

void Config::reset() {
    Serial.println("[Config] Resetting to defaults");

    // Reset Pomodoro to defaults
    pomodoro = PomodoroSettings();

    // Reset UI to defaults
    ui = UISettings();

    // Reset Network to defaults
    network = NetworkSettings();

    // Reset Power to defaults
    power = PowerSettings();

    dirty = true;
    save();
}

void Config::setPomodoro(const PomodoroSettings& settings) {
    pomodoro = settings;
    dirty = true;
    Serial.println("[Config] Pomodoro settings updated");
}

void Config::setUI(const UISettings& settings) {
    ui = settings;
    dirty = true;
    Serial.println("[Config] UI settings updated");
}

void Config::setNetwork(const NetworkSettings& settings) {
    network = settings;
    dirty = true;
    Serial.println("[Config] Network settings updated");
}

void Config::setPower(const PowerSettings& settings) {
    power = settings;
    dirty = true;
    Serial.println("[Config] Power settings updated");
}
