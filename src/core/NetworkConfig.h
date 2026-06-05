#ifndef NETWORK_CONFIG_H
#define NETWORK_CONFIG_H

#include "../hardware/SDManager.h"
#include <cstdint>
#include <cstring>

/**
 * @brief Network configuration management with SD card storage
 *
 * Loads network settings from SD card /config/network.ini (INI format)
 * and SSL certificates from /config/certs/*.pem (PEM format).
 *
 * Separation strategy (MP-72):
 * - NetworkConfig (SD card): Deployment settings (WiFi, MQTT, certificates)
 * - Config (NVS): User preferences (pomodoro settings, UI, power)
 *
 * File structure:
 * - /config/network.ini - WiFi, MQTT, CloudSync, NTP configuration
 * - /config/certs/AmazonRootCA1.pem - AWS IoT Root CA certificate
 * - /config/certs/device-certificate.pem.crt - Device certificate
 * - /config/certs/device-private.pem.key - Private key (SECRET)
 *
 * Features:
 * - INI parsing with minIniFS library
 * - SSL certificate loading into PSRAM (fallback to heap)
 * - PEM format validation
 * - Graceful degradation if SD unavailable
 *
 * Usage:
 *   NetworkConfig netConfig(sdManager);
 *   if (netConfig.load()) {
 *       if (netConfig.loadCertificates()) {
 *           wifiClientSecure.setCACert(netConfig.getRootCA());
 *       }
 *   }
 */
class NetworkConfig {
public:
    /**
     * WiFi network credentials and settings
     */
    struct WiFiSettings {
        char ssid[32] = "";                   // WiFi SSID
        char password[64] = "";               // WiFi password
        char ssid_fallback[32] = "";          // Optional fallback network
        char password_fallback[64] = "";      // Optional fallback password
    };

    /**
     * MQTT broker configuration (AWS IoT Core)
     */
    struct MQTTSettings {
        char broker[128] = "";                // MQTT broker hostname (e.g., xxx.iot.region.amazonaws.com)
        uint16_t port = 8883;                 // MQTT port (8883 for TLS)
        char client_id[32] = "";              // MQTT client ID
        char thing_name[32] = "";             // AWS IoT Thing name (shadow topic). Defaults to client_id if empty.
        uint16_t keepalive = 60;              // Keep-alive interval (seconds)
    };

    /**
     * SSL/TLS certificate file paths (relative to SD root)
     */
    struct CertPaths {
        char root_ca[64] = "";                // Path to root CA cert (e.g., /config/certs/AmazonRootCA1.pem)
        char device_cert[64] = "";            // Path to device cert (e.g., /config/certs/device-certificate.pem.crt)
        char private_key[64] = "";            // Path to private key (e.g., /config/certs/device-private.pem.key)
    };

    /**
     * Cloud synchronization settings
     */
    struct CloudSyncSettings {
        bool enabled = false;                 // Enable/disable cloud sync
        uint16_t sync_interval_min = 5;       // Sync interval in minutes
    };

    /**
     * Single webhook endpoint loaded from a [Webhook.N] section of network.ini.
     * Up to MAX_WEBHOOKS may be configured. enabled is true only when URL is
     * populated. event_mask is a bitmask of SessionEvent values (see
     * network/SessionEvent.h); SESSION_EVENT_ALL means "fire on every event".
     */
    static constexpr uint8_t MAX_WEBHOOKS = 4;

    // Payload shape selector. JSON (default) sends our generic SessionEvent
    // JSON; TELEGRAM sends {chat_id, text} with the text rendered from the
    // Text= template — saves users from running a relay.
    enum class WebhookFormat : uint8_t {
        JSON = 0,
        TELEGRAM = 1,
    };

    struct WebhookEndpoint {
        bool enabled = false;
        uint8_t event_mask = 0;
        WebhookFormat format = WebhookFormat::JSON;
        char url[192] = "";
        char auth_header[128] = "";   // Full Authorization header value, e.g. "Bearer xxx"
        char chat_id[24] = "";        // Required when format=TELEGRAM (numeric; negative for groups)
        char text_template[256] = ""; // Template with {placeholders}; empty = sensible default
    };

    struct WebhookSettings {
        WebhookEndpoint endpoints[MAX_WEBHOOKS];
        uint8_t count = 0;  // Number of populated endpoints
    };

    /**
     * NTP time synchronization settings
     *
     * Timezone resolution precedence (computed once at load() time into
     * resolved_tz, a POSIX TZ string suitable for setenv("TZ", ...)):
     *   1. TimezoneOffset set explicitly → fixed offset (+ EU DST rules if DST=true)
     *   2. Timezone set → IANA name lookup, else used as-is (assumed POSIX TZ)
     *   3. Neither set → "UTC0"
     */
    struct NTPSettings {
        char server[64] = "pool.ntp.org";     // NTP server hostname
        int32_t timezone_offset = 0;          // Legacy: fixed seconds from UTC
        bool timezone_offset_set = false;     // True if TimezoneOffset key present in INI
        char timezone_name[64] = "";          // IANA name (e.g. Europe/London) or raw POSIX TZ
        bool dst_enabled = false;             // Apply auto-DST atop TimezoneOffset (EU rules)
        char resolved_tz[64] = "UTC0";        // Final POSIX TZ string passed to setenv
    };

    /**
     * Constructor
     * @param sdManager Reference to SDManager for file operations
     */
    NetworkConfig(SDManager& sdManager);

    /**
     * Destructor - frees certificate buffers
     */
    ~NetworkConfig();

    /**
     * Load network configuration from /config/network.ini
     * @return true if loaded successfully, false otherwise
     */
    bool load();

    /**
     * Load SSL certificates from SD card into memory (PSRAM preferred)
     * @return true if all certificates loaded successfully, false otherwise
     */
    bool loadCertificates();

    /**
     * Get WiFi settings
     */
    const WiFiSettings& getWiFi() const { return wifi; }

    /**
     * Get MQTT settings
     */
    const MQTTSettings& getMQTT() const { return mqtt; }

    /**
     * Get certificate file paths
     */
    const CertPaths& getCertPaths() const { return cert_paths; }

    /**
     * Get cloud sync settings
     */
    const CloudSyncSettings& getCloudSync() const { return cloud_sync; }

    /**
     * Get NTP settings
     */
    const NTPSettings& getNTP() const { return ntp; }

    /**
     * Get configured webhook endpoints (up to MAX_WEBHOOKS).
     */
    const WebhookSettings& getWebhooks() const { return webhooks; }

    /**
     * Get root CA certificate content (PEM format)
     * @return Null-terminated PEM string, or nullptr if not loaded
     */
    const char* getRootCA() const { return root_ca_content; }

    /**
     * Install an externally-owned PEM string as the root CA. Used by main.cpp
     * to fall back to the embedded Amazon Root CA when SD didn't provide one.
     * The buffer must outlive NetworkConfig (typically a static const). Does
     * NOT take ownership and will not free() it in freeBuffers().
     */
    void setRootCAFallback(const char* pem);

    /**
     * Get device certificate content (PEM format)
     * @return Null-terminated PEM string, or nullptr if not loaded
     */
    const char* getDeviceCert() const { return device_cert_content; }

    /**
     * Get private key content (PEM format)
     * @return Null-terminated PEM string, or nullptr if not loaded
     */
    const char* getPrivateKey() const { return private_key_content; }

    /**
     * Check if configuration loaded successfully
     */
    bool isValid() const { return loaded; }

    /**
     * Check if certificates loaded successfully
     */
    bool hasCertificates() const { return certs_loaded; }

private:
    SDManager& sd;                    // Reference to SDManager
    bool loaded = false;              // Configuration loaded flag
    bool certs_loaded = false;        // Certificates loaded flag

    // Configuration data
    WiFiSettings wifi;
    MQTTSettings mqtt;
    CertPaths cert_paths;
    CloudSyncSettings cloud_sync;
    NTPSettings ntp;
    WebhookSettings webhooks;

    // Certificate content buffers (allocated from PSRAM if available)
    char* root_ca_content = nullptr;
    char* device_cert_content = nullptr;
    char* private_key_content = nullptr;
    // True when root_ca_content points at a static buffer owned by someone
    // else (e.g. the embedded Amazon Root CA) — freeBuffers() must skip free().
    bool root_ca_external = false;

    // Constants
    static constexpr const char* CONFIG_FILE = "/config/network.ini";
    static constexpr size_t MAX_CERT_SIZE = 2048;  // Max certificate size (bytes)

    // Helper methods
    bool loadIniFile();
    bool loadCertFile(const char* path, char** buffer, size_t max_size);
    bool validatePEMFormat(const char* content, const char* expected_header);
    void freeBuffers();

    // Resolve [NTP] timezone keys into a POSIX TZ string in ntp.resolved_tz.
    // TimezoneOffset wins over Timezone if both are present. With DST=true,
    // a fixed offset gets EU DST rules (last Sun of Mar -> last Sun of Oct).
    void resolveTZ();
    // Look up an IANA-style name (e.g. "Europe/London") in the built-in
    // table and return its POSIX TZ string, or nullptr if not found.
    static const char* mapIanaToPosix(const char* iana_name);
};

#endif // NETWORK_CONFIG_H
