#include "NetworkConfig.h"
#include "../network/SessionEvent.h"
#include "../network/WebhookDispatcher.h"  // for maskUrl
#include <Arduino.h>
#include <minIniFS.h>
#include <esp_heap_caps.h>
#include <TzDbLookup.h>  // ~470 IANA -> POSIX entries in PROGMEM

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

    // ThingName drives the shadow topic ($aws/things/<name>/shadow/...).
    // Defaults to client_id since that's the common case.
    String thing_name = ini.gets("MQTT", "ThingName", "");
    if (thing_name.length() > 0) {
        strncpy(mqtt.thing_name, thing_name.c_str(), sizeof(mqtt.thing_name) - 1);
    } else {
        strncpy(mqtt.thing_name, mqtt.client_id, sizeof(mqtt.thing_name) - 1);
    }
    mqtt.thing_name[sizeof(mqtt.thing_name) - 1] = '\0';

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

    // Webhook endpoints: [Webhook.1] .. [Webhook.MAX_WEBHOOKS]. Stop at the
    // first section that has no URL.
    webhooks.count = 0;
    for (uint8_t i = 0; i < MAX_WEBHOOKS; ++i) {
        char section[16];
        snprintf(section, sizeof(section), "Webhook.%u", static_cast<unsigned>(i + 1));

        String url_s = ini.gets(section, "URL", "");
        if (url_s.length() == 0) continue;

        WebhookEndpoint& w = webhooks.endpoints[webhooks.count++];
        w.enabled = true;
        strncpy(w.url, url_s.c_str(), sizeof(w.url) - 1);
        w.url[sizeof(w.url) - 1] = '\0';

        String events_s = ini.gets(section, "Events", "*");
        w.event_mask = parseSessionEventMask(events_s.c_str());

        String auth_s = ini.gets(section, "AuthHeader", "");
        strncpy(w.auth_header, auth_s.c_str(), sizeof(w.auth_header) - 1);
        w.auth_header[sizeof(w.auth_header) - 1] = '\0';

        // Optional Format= selector. Unknown values fall back to JSON.
        String fmt_s = ini.gets(section, "Format", "json");
        fmt_s.toLowerCase();
        w.format = (fmt_s == "telegram") ? WebhookFormat::TELEGRAM : WebhookFormat::JSON;

        String chat_s = ini.gets(section, "ChatID", "");
        strncpy(w.chat_id, chat_s.c_str(), sizeof(w.chat_id) - 1);
        w.chat_id[sizeof(w.chat_id) - 1] = '\0';

        String text_s = ini.gets(section, "Text", "");
        strncpy(w.text_template, text_s.c_str(), sizeof(w.text_template) - 1);
        w.text_template[sizeof(w.text_template) - 1] = '\0';

        if (w.format == WebhookFormat::TELEGRAM && w.chat_id[0] == '\0') {
            Serial.printf("[NetworkConfig] WARN: Webhook[%u] format=telegram but no ChatID; will be skipped\n",
                          static_cast<unsigned>(i + 1));
        }

        char masked[208];
        WebhookDispatcher::maskUrl(w.url, masked, sizeof(masked));
        Serial.printf("[NetworkConfig] Webhook[%u]: %s (events=0x%02X, format=%s%s)\n",
                      static_cast<unsigned>(i + 1), masked, w.event_mask,
                      w.format == WebhookFormat::TELEGRAM ? "telegram" : "json",
                      w.auth_header[0] ? ", auth" : "");
    }

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

    // Allocate buffer. Prefer INTERNAL heap (not PSRAM) — mbedtls's TLS
    // handshake hangs/fails when WiFiClientSecure reads PEM bytes from
    // PSRAM-backed buffers (cache/DMA coherency issue on ESP32). Cert + key
    // total ~3 KB; internal heap has plenty. PSRAM is a fallback only if
    // the internal heap is exhausted.
    size_t alloc_size = content.length() + 1;  // +1 for null terminator

    *buffer = (char*)heap_caps_malloc(alloc_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (*buffer != nullptr) {
        Serial.printf("[NetworkConfig] Allocated %d bytes in internal heap for %s\n",
                      alloc_size, path);
    } else if (psramFound()) {
        // Internal heap exhausted — try PSRAM. TLS handshake may misbehave.
        *buffer = (char*)heap_caps_malloc(alloc_size, MALLOC_CAP_SPIRAM);
        if (*buffer != nullptr) {
            Serial.printf("[NetworkConfig] WARN: fell back to PSRAM for %s — TLS may fail\n",
                          path);
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

void NetworkConfig::setRootCAFallback(const char* pem) {
    if (!pem) return;
    // If we already own a heap/PSRAM buffer for the CA, release it before
    // pointing at the external (static) PEM.
    if (root_ca_content != nullptr && !root_ca_external) {
        free(root_ca_content);
    }
    root_ca_content = const_cast<char*>(pem);
    root_ca_external = true;
}

void NetworkConfig::freeBuffers() {
    if (root_ca_content != nullptr) {
        // Skip free() when the CA buffer is owned externally (embedded PEM).
        if (!root_ca_external) free(root_ca_content);
        root_ca_content = nullptr;
        root_ca_external = false;
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

const char* NetworkConfig::mapIanaToPosix(const char* iana_name) {
    if (!iana_name) return nullptr;
    // Bare aliases not present in the IANA DB ("Etc/UTC" is the canonical
    // entry; users still write "UTC" or "GMT").
    if (strcasecmp(iana_name, "UTC") == 0) return "UTC0";
    if (strcasecmp(iana_name, "GMT") == 0) return "GMT0";
    // Full IANA database (~470 zones) lives in TzDbLookup's PROGMEM table.
    return TzDbLookup::getPosix(iana_name);
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
        if (!posix && strchr(ntp.timezone_name, '/')) {
            // Looks like an IANA name (has '/') but wasn't in the IANA DB
            // (TzDbLookup). newlib's tzset() doesn't understand IANA — it'll
            // silently fall back to UTC. Surface this so the wrong-clock bug
            // is obvious (almost always a typo at this point).
            Serial.printf("[NetworkConfig] WARN: timezone \"%s\" not found in IANA database; "
                          "check spelling, or use TimezoneOffset= / a raw POSIX TZ string.\n",
                          ntp.timezone_name);
        }
        strncpy(ntp.resolved_tz, src, sizeof(ntp.resolved_tz) - 1);
        ntp.resolved_tz[sizeof(ntp.resolved_tz) - 1] = '\0';
        return;
    }

    strcpy(ntp.resolved_tz, "UTC0");
}
