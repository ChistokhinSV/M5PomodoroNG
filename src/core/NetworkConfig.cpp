#include "NetworkConfig.h"
#include <Arduino.h>
#include <minIniFS.h>
#include <esp_heap_caps.h>

NetworkConfig::NetworkConfig(SDManager& sdManager)
    : sd(sdManager),
      loaded(false),
      certs_loaded(false),
      root_ca_content(nullptr),
      device_cert_content(nullptr),
      private_key_content(nullptr) {
}

NetworkConfig::~NetworkConfig() {
    freeBuffers();
}

bool NetworkConfig::load() {
    if (!sd.isMounted()) {
        Serial.println("[NetworkConfig] ERROR: SD card not mounted");
        return false;
    }

    if (!sd.exists(CONFIG_FILE)) {
        Serial.printf("[NetworkConfig] ERROR: %s not found\n", CONFIG_FILE);
        Serial.println("[NetworkConfig] Copy config/network.ini.template to SD:/config/network.ini");
        return false;
    }

    if (!loadIniFile()) {
        Serial.println("[NetworkConfig] ERROR: Failed to parse network.ini");
        return false;
    }

    loaded = true;
    Serial.println("[NetworkConfig] Configuration loaded successfully");
    return true;
}

bool NetworkConfig::loadIniFile() {
    // Initialize minIniFS with SD filesystem
    minIniFS ini(CONFIG_FILE);
    ini_FS(SD);

    Serial.printf("[NetworkConfig] Loading %s...\n", CONFIG_FILE);

    // Load WiFi settings
    String ssid = ini.gets("WiFi", "SSID", "");
    if (ssid.length() > 0) {
        strncpy(wifi.ssid, ssid.c_str(), sizeof(wifi.ssid) - 1);
        wifi.ssid[sizeof(wifi.ssid) - 1] = '\0';
    }

    String password = ini.gets("WiFi", "Password", "");
    if (password.length() > 0) {
        strncpy(wifi.password, password.c_str(), sizeof(wifi.password) - 1);
        wifi.password[sizeof(wifi.password) - 1] = '\0';
    }

    String ssid_fallback = ini.gets("WiFi", "SSID_Fallback", "");
    if (ssid_fallback.length() > 0) {
        strncpy(wifi.ssid_fallback, ssid_fallback.c_str(), sizeof(wifi.ssid_fallback) - 1);
        wifi.ssid_fallback[sizeof(wifi.ssid_fallback) - 1] = '\0';
    }

    String password_fallback = ini.gets("WiFi", "Password_Fallback", "");
    if (password_fallback.length() > 0) {
        strncpy(wifi.password_fallback, password_fallback.c_str(), sizeof(wifi.password_fallback) - 1);
        wifi.password_fallback[sizeof(wifi.password_fallback) - 1] = '\0';
    }

    // Load MQTT settings
    String broker = ini.gets("MQTT", "Broker", "");
    if (broker.length() > 0) {
        strncpy(mqtt.broker, broker.c_str(), sizeof(mqtt.broker) - 1);
        mqtt.broker[sizeof(mqtt.broker) - 1] = '\0';
    }

    mqtt.port = ini.geti("MQTT", "Port", 8883);

    String client_id = ini.gets("MQTT", "ClientID", "");
    if (client_id.length() > 0) {
        strncpy(mqtt.client_id, client_id.c_str(), sizeof(mqtt.client_id) - 1);
        mqtt.client_id[sizeof(mqtt.client_id) - 1] = '\0';
    }

    mqtt.keepalive = ini.geti("MQTT", "KeepAlive", 60);

    // Load certificate paths
    String root_ca = ini.gets("Certificates", "RootCA", "");
    if (root_ca.length() > 0) {
        strncpy(cert_paths.root_ca, root_ca.c_str(), sizeof(cert_paths.root_ca) - 1);
        cert_paths.root_ca[sizeof(cert_paths.root_ca) - 1] = '\0';
    }

    String device_cert = ini.gets("Certificates", "DeviceCert", "");
    if (device_cert.length() > 0) {
        strncpy(cert_paths.device_cert, device_cert.c_str(), sizeof(cert_paths.device_cert) - 1);
        cert_paths.device_cert[sizeof(cert_paths.device_cert) - 1] = '\0';
    }

    String private_key = ini.gets("Certificates", "PrivateKey", "");
    if (private_key.length() > 0) {
        strncpy(cert_paths.private_key, private_key.c_str(), sizeof(cert_paths.private_key) - 1);
        cert_paths.private_key[sizeof(cert_paths.private_key) - 1] = '\0';
    }

    // Load CloudSync settings
    cloud_sync.enabled = ini.getbool("CloudSync", "Enabled", false);
    cloud_sync.sync_interval_min = ini.geti("CloudSync", "SyncIntervalMin", 5);

    // Load NTP settings
    String ntp_server = ini.gets("NTP", "Server", "pool.ntp.org");
    if (ntp_server.length() > 0) {
        strncpy(ntp.server, ntp_server.c_str(), sizeof(ntp.server) - 1);
        ntp.server[sizeof(ntp.server) - 1] = '\0';
    }

    // TimezoneOffset is read as string so we can distinguish "missing" (use
    // Timezone instead) from "= 0" (explicit UTC fixed offset).
    String tz_offset_str = ini.gets("NTP", "TimezoneOffset", "");
    ntp.timezone_offset_set = tz_offset_str.length() > 0;
    ntp.timezone_offset = ntp.timezone_offset_set ? tz_offset_str.toInt() : 0;

    String tz_name = ini.gets("NTP", "Timezone", "");
    if (tz_name.length() > 0) {
        strncpy(ntp.timezone_name, tz_name.c_str(), sizeof(ntp.timezone_name) - 1);
        ntp.timezone_name[sizeof(ntp.timezone_name) - 1] = '\0';
    }

    ntp.dst_enabled = ini.getbool("NTP", "DST", false);

    // Pick which input wins, build the POSIX TZ string for setenv("TZ", ...).
    resolveTZ();

    // Validate required settings
    if (strlen(wifi.ssid) == 0) {
        Serial.println("[NetworkConfig] ERROR: WiFi SSID not configured");
        return false;
    }

    if (strlen(mqtt.broker) == 0) {
        Serial.println("[NetworkConfig] WARN: MQTT broker not configured");
    }

    Serial.printf("[NetworkConfig] WiFi: %s\n", wifi.ssid);
    Serial.printf("[NetworkConfig] MQTT: %s:%d\n", mqtt.broker, mqtt.port);
    Serial.printf("[NetworkConfig] CloudSync: %s (interval: %d min)\n",
                  cloud_sync.enabled ? "enabled" : "disabled",
                  cloud_sync.sync_interval_min);
    Serial.printf("[NetworkConfig] NTP: %s (TZ=\"%s\")\n",
                  ntp.server, ntp.resolved_tz);

    return true;
}

bool NetworkConfig::loadCertificates() {
    if (!loaded) {
        Serial.println("[NetworkConfig] ERROR: Load configuration first with load()");
        return false;
    }

    // Check if certificate paths configured
    if (strlen(cert_paths.root_ca) == 0 ||
        strlen(cert_paths.device_cert) == 0 ||
        strlen(cert_paths.private_key) == 0) {
        Serial.println("[NetworkConfig] WARN: Certificate paths not configured");
        return false;
    }

    Serial.println("[NetworkConfig] Loading SSL certificates...");

    // Load root CA certificate
    if (!loadCertFile(cert_paths.root_ca, &root_ca_content, MAX_CERT_SIZE)) {
        Serial.printf("[NetworkConfig] ERROR: Failed to load root CA: %s\n", cert_paths.root_ca);
        freeBuffers();
        return false;
    }

    if (!validatePEMFormat(root_ca_content, "-----BEGIN CERTIFICATE-----")) {
        Serial.printf("[NetworkConfig] ERROR: Invalid PEM format for root CA\n");
        freeBuffers();
        return false;
    }

    // Load device certificate
    if (!loadCertFile(cert_paths.device_cert, &device_cert_content, MAX_CERT_SIZE)) {
        Serial.printf("[NetworkConfig] ERROR: Failed to load device cert: %s\n", cert_paths.device_cert);
        freeBuffers();
        return false;
    }

    if (!validatePEMFormat(device_cert_content, "-----BEGIN CERTIFICATE-----")) {
        Serial.printf("[NetworkConfig] ERROR: Invalid PEM format for device cert\n");
        freeBuffers();
        return false;
    }

    // Load private key
    if (!loadCertFile(cert_paths.private_key, &private_key_content, MAX_CERT_SIZE)) {
        Serial.printf("[NetworkConfig] ERROR: Failed to load private key: %s\n", cert_paths.private_key);
        freeBuffers();
        return false;
    }

    if (!validatePEMFormat(private_key_content, "-----BEGIN")) {
        Serial.printf("[NetworkConfig] ERROR: Invalid PEM format for private key\n");
        freeBuffers();
        return false;
    }

    certs_loaded = true;
    Serial.println("[NetworkConfig] SSL certificates loaded successfully");
    Serial.printf("[NetworkConfig] Root CA: %d bytes\n", strlen(root_ca_content));
    Serial.printf("[NetworkConfig] Device cert: %d bytes\n", strlen(device_cert_content));
    Serial.printf("[NetworkConfig] Private key: %d bytes\n", strlen(private_key_content));

    return true;
}

bool NetworkConfig::loadCertFile(const char* path, char** buffer, size_t max_size) {
    if (!sd.exists(path)) {
        Serial.printf("[NetworkConfig] ERROR: Certificate file not found: %s\n", path);
        return false;
    }

    // Read file content
    String content = sd.readFile(path);
    if (content.length() == 0) {
        Serial.printf("[NetworkConfig] ERROR: Failed to read certificate file: %s\n", path);
        return false;
    }

    if (content.length() > max_size) {
        Serial.printf("[NetworkConfig] ERROR: Certificate file too large: %d bytes (max: %d)\n",
                      content.length(), max_size);
        return false;
    }

    // Allocate buffer (prefer PSRAM if available)
    size_t alloc_size = content.length() + 1;  // +1 for null terminator

    if (psramFound()) {
        *buffer = (char*)heap_caps_malloc(alloc_size, MALLOC_CAP_SPIRAM);
        if (*buffer != nullptr) {
            Serial.printf("[NetworkConfig] Allocated %d bytes in PSRAM for %s\n",
                         alloc_size, path);
        }
    }

    // Fallback to regular heap if PSRAM unavailable or allocation failed
    if (*buffer == nullptr) {
        *buffer = (char*)malloc(alloc_size);
        if (*buffer != nullptr) {
            Serial.printf("[NetworkConfig] Allocated %d bytes in heap for %s\n",
                         alloc_size, path);
        }
    }

    if (*buffer == nullptr) {
        Serial.printf("[NetworkConfig] ERROR: Failed to allocate memory for %s\n", path);
        return false;
    }

    // Copy content to buffer
    strcpy(*buffer, content.c_str());

    return true;
}

bool NetworkConfig::validatePEMFormat(const char* content, const char* expected_header) {
    if (content == nullptr || expected_header == nullptr) {
        return false;
    }

    // Check if content starts with expected PEM header
    if (strstr(content, expected_header) == nullptr) {
        Serial.printf("[NetworkConfig] ERROR: Invalid PEM format (expected %s)\n",
                     expected_header);
        return false;
    }

    return true;
}

void NetworkConfig::freeBuffers() {
    if (root_ca_content != nullptr) {
        free(root_ca_content);
        root_ca_content = nullptr;
    }

    if (device_cert_content != nullptr) {
        free(device_cert_content);
        device_cert_content = nullptr;
    }

    if (private_key_content != nullptr) {
        free(private_key_content);
        private_key_content = nullptr;
    }

    certs_loaded = false;
    Serial.println("[NetworkConfig] Certificate buffers freed");
}

// --- Timezone resolution -----------------------------------------------------

namespace {
// IANA name -> POSIX TZ. POSIX sign convention is WEST-of-UTC positive, so
// CET (UTC+1) is "CET-1" and EST (UTC-5) is "EST5". DST rules are encoded as
// Mmonth.week.day[/hour]. Built-in pool covers the common cases; users with
// exotic zones can paste a POSIX TZ string directly into Timezone= instead.
struct IanaEntry { const char* iana; const char* posix; };
constexpr IanaEntry IANA_TABLE[] = {
    {"UTC",                 "UTC0"},
    {"GMT",                 "GMT0"},
    {"Europe/London",       "GMT0BST,M3.5.0/1,M10.5.0/2"},
    {"Europe/Dublin",       "GMT0IST,M3.5.0/1,M10.5.0/2"},
    {"Europe/Lisbon",       "WET0WEST,M3.5.0/1,M10.5.0/2"},
    {"Europe/Berlin",       "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Paris",        "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Madrid",       "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Rome",         "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Amsterdam",    "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Brussels",     "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Vienna",       "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Warsaw",       "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Prague",       "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Stockholm",    "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Helsinki",     "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"Europe/Athens",       "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"Europe/Bucharest",    "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"Europe/Kiev",         "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"Europe/Kyiv",         "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"Europe/Riga",         "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"Europe/Tallinn",      "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"Europe/Vilnius",      "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"Europe/Moscow",       "MSK-3"},
    {"Europe/Istanbul",     "TRT-3"},
    {"America/New_York",    "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Chicago",     "CST6CDT,M3.2.0,M11.1.0"},
    {"America/Denver",      "MST7MDT,M3.2.0,M11.1.0"},
    {"America/Phoenix",     "MST7"},
    {"America/Los_Angeles", "PST8PDT,M3.2.0,M11.1.0"},
    {"America/Anchorage",   "AKST9AKDT,M3.2.0,M11.1.0"},
    {"America/Honolulu",    "HST10"},
    {"America/Toronto",     "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Vancouver",   "PST8PDT,M3.2.0,M11.1.0"},
    {"America/Mexico_City", "CST6CDT,M4.1.0,M10.5.0"},
    {"America/Sao_Paulo",   "BRT3"},
    {"America/Buenos_Aires","ART3"},
    {"Asia/Tokyo",          "JST-9"},
    {"Asia/Shanghai",       "CST-8"},
    {"Asia/Hong_Kong",      "HKT-8"},
    {"Asia/Singapore",      "SGT-8"},
    {"Asia/Seoul",          "KST-9"},
    {"Asia/Bangkok",        "ICT-7"},
    {"Asia/Kolkata",        "IST-5:30"},
    {"Asia/Dubai",          "GST-4"},
    {"Asia/Jerusalem",      "IST-2IDT,M3.4.4/26,M10.5.0"},
    {"Australia/Sydney",    "AEST-10AEDT,M10.1.0,M4.1.0/3"},
    {"Australia/Melbourne", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
    {"Australia/Brisbane",  "AEST-10"},
    {"Australia/Perth",     "AWST-8"},
    {"Australia/Adelaide",  "ACST-9:30ACDT,M10.1.0,M4.1.0/3"},
    {"Pacific/Auckland",    "NZST-12NZDT,M9.5.0,M4.1.0/3"},
};
}  // namespace

const char* NetworkConfig::mapIanaToPosix(const char* iana_name) {
    if (!iana_name) return nullptr;
    for (const auto& e : IANA_TABLE) {
        if (strcmp(e.iana, iana_name) == 0) return e.posix;
    }
    return nullptr;
}

void NetworkConfig::resolveTZ() {
    if (ntp.timezone_offset_set) {
        // Manual offset wins. POSIX TZ sign is INVERTED (west-positive), so
        // UTC+1 (3600s east) becomes "LOC-1" and UTC-5 (-18000s) becomes "LOC5".
        int posix_total_min = -(ntp.timezone_offset / 60);
        int hh = posix_total_min / 60;
        int mm = posix_total_min >= 0 ? (posix_total_min % 60) : (-posix_total_min % 60);

        char std_tail[24];
        if (mm == 0) {
            snprintf(std_tail, sizeof(std_tail), "%d", hh);
        } else {
            snprintf(std_tail, sizeof(std_tail), "%d:%02d", hh, mm);
        }

        if (ntp.dst_enabled) {
            // EU default DST: last Sun of Mar at 02:00 → last Sun of Oct at 03:00.
            // POSIX assumes 1-hour DST shift by default (no offset after DST name).
            snprintf(ntp.resolved_tz, sizeof(ntp.resolved_tz),
                     "LOC%sDST,M3.5.0/2,M10.5.0/3", std_tail);
        } else {
            snprintf(ntp.resolved_tz, sizeof(ntp.resolved_tz), "LOC%s", std_tail);
        }
        return;
    }

    if (ntp.timezone_name[0] != '\0') {
        const char* posix = mapIanaToPosix(ntp.timezone_name);
        const char* src = posix ? posix : ntp.timezone_name;  // Treat unknown as raw POSIX
        strncpy(ntp.resolved_tz, src, sizeof(ntp.resolved_tz) - 1);
        ntp.resolved_tz[sizeof(ntp.resolved_tz) - 1] = '\0';
        return;
    }

    strcpy(ntp.resolved_tz, "UTC0");
}
