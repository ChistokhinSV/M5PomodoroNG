#include "MQTTClient.h"
#include <Arduino.h>

namespace {
// PubSubClient's callback is a C function pointer with no userdata. We assume
// at most one active MQTTClient instance at a time (true in this project) and
// keep a process-global pointer for the trampoline to dispatch through.
MQTTClient* g_active = nullptr;
}  // namespace

MQTTClient::MQTTClient(const NetworkConfig::MQTTSettings& cfg,
                       const char* root_ca,
                       const char* device_cert,
                       const char* private_key)
    : cfg_(cfg),
      root_ca_(root_ca),
      device_cert_(device_cert),
      private_key_(private_key),
      mqtt_(tls_) {
}

void MQTTClient::begin() {
    if (began_) return;
    g_active = this;

    // Mutual TLS — AWS IoT requires all three: trust anchor (root CA), client
    // cert (per-device), client key (per-device).
    if (root_ca_)     tls_.setCACert(root_ca_);
    if (device_cert_) tls_.setCertificate(device_cert_);
    if (private_key_) tls_.setPrivateKey(private_key_);
    // Default is ~15s; AWS IoT handshakes can take longer on slow links and
    // we want to outlive a slow EU edge. 30s also gives mbedtls room for the
    // full ClientHello/Cert/Verify exchange before WiFiClientSecure gives up.
    tls_.setHandshakeTimeout(30);

    mqtt_.setServer(cfg_.broker, cfg_.port);
    mqtt_.setKeepAlive(cfg_.keepalive ? cfg_.keepalive : 60);
    mqtt_.setBufferSize(1024);   // shadow updates ~200-400B; deltas can grow
    mqtt_.setCallback(&MQTTClient::onMqttMessage);

    began_ = true;
    Serial.printf("[MQTT] begin: broker=%s:%u client=%s keepalive=%us\n",
                  cfg_.broker, cfg_.port, cfg_.client_id, cfg_.keepalive);
}

bool MQTTClient::connect() {
    if (!began_) begin();
    if (mqtt_.connected()) return true;

    Serial.printf("[MQTT] Connecting to %s:%u as %s ...\n",
                  cfg_.broker, cfg_.port, cfg_.client_id);
    uint32_t t0 = millis();
    bool ok = mqtt_.connect(cfg_.client_id);
    uint32_t dt = millis() - t0;

    if (ok) {
        Serial.printf("[MQTT] Connected as %s in %lu ms\n", cfg_.client_id, (unsigned long)dt);
    } else {
        // PubSubClient state codes: -4 timeout, -3 conn lost, -2 conn failed,
        // -1 disconnected, 1 bad protocol, 2 bad client id, 3 unavailable,
        // 4 bad credentials, 5 unauthorized.
        int st = mqtt_.state();
        Serial.printf("[MQTT] Connect FAILED (state=%d) after %lu ms\n",
                      st, (unsigned long)dt);

        // Surface the mbedtls error that WiFiClientSecure captured during the
        // handshake. The most common cases for state=-1 with AWS IoT:
        //   -0x2700 / X509_CERT_VERIFY_FAILED — CA mismatch (wrong/expired root)
        //   -0x7780 / SSL_FATAL_ALERT_MESSAGE — server alerted (often policy
        //              denies iot:Connect on this client_id, AWS replies with
        //              a fatal "access_denied" alert before MQTT CONNACK)
        //   -0x6900 / NET_RECV_FAILED — peer closed mid-handshake
        // The text form is more useful than the code; lastError writes both
        // when an error buffer is provided.
        char err_buf[128] = {0};
        int err = tls_.lastError(err_buf, sizeof(err_buf));
        if (err != 0 || err_buf[0]) {
            Serial.printf("[MQTT] TLS error 0x%04X: %s\n",
                          err < 0 ? -err : err, err_buf[0] ? err_buf : "(no detail)");
        } else {
            Serial.println("[MQTT] No TLS error captured (peer closed without alert?)");
        }
        // tls_.connected() lets us tell socket-level dropped vs TLS error.
        Serial.printf("[MQTT] TLS socket state: connected=%d\n",
                      tls_.connected() ? 1 : 0);
    }
    return ok;
}

bool MQTTClient::isConnected() { return mqtt_.connected(); }

void MQTTClient::loop() {
    if (!began_) return;
    mqtt_.loop();
}

void MQTTClient::disconnect() {
    if (began_) mqtt_.disconnect();
}

bool MQTTClient::publish(const char* topic, const char* payload) {
    if (!mqtt_.connected()) return false;
    return mqtt_.publish(topic, payload);
}

bool MQTTClient::subscribe(const char* topic) {
    if (!mqtt_.connected()) return false;
    return mqtt_.subscribe(topic);
}

void MQTTClient::onMqttMessage(char* topic, uint8_t* payload, unsigned int len) {
    if (!g_active || !g_active->cb_) return;
    // PubSubClient's payload buffer is owned by the library and contains the
    // raw bytes (no null terminator). Forward as a const char* + length; the
    // callback can copy if it needs persistence.
    g_active->cb_(topic, reinterpret_cast<const char*>(payload), len);
}
