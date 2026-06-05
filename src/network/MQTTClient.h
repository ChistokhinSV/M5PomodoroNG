#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include "../core/NetworkConfig.h"
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
// PubSubClient defines MQTT_CONNECTED, MQTT_DISCONNECTED, etc. as preprocessor
// macros at file scope. They collide with our NetworkStatus::Event enum values
// of the same name. Undef them here so anyone who transitively pulls
// PubSubClient via this header isn't poisoned.
#undef MQTT_CONNECTION_TIMEOUT
#undef MQTT_CONNECTION_LOST
#undef MQTT_CONNECT_FAILED
#undef MQTT_DISCONNECTED
#undef MQTT_CONNECTED
#undef MQTT_CONNECT_BAD_PROTOCOL
#undef MQTT_CONNECT_BAD_CLIENT_ID
#undef MQTT_CONNECT_UNAVAILABLE
#undef MQTT_CONNECT_BAD_CREDENTIALS
#undef MQTT_CONNECT_UNAUTHORIZED
#include <functional>

/**
 * Thin wrapper around PubSubClient + WiFiClientSecure that owns the mutual-TLS
 * session to AWS IoT Core. Lives on Core 1 and is driven by NetworkTask.
 *
 * Why a wrapper:
 *   - PubSubClient's message callback is a C-style function pointer; we want
 *     std::function so the consumer (ShadowPublisher) can bind to a member.
 *   - WiFiClientSecure cert/key setup is finicky; we centralize it here.
 *   - We need a single "is everything up?" view that combines TLS + MQTT
 *     handshake state for NetworkTask's status reporting.
 *
 * Cert pointers are not owned (they live in NetworkConfig PSRAM buffers or in
 * the embedded AmazonRootCA.h constant). They must outlive this instance.
 */
class MQTTClient {
public:
    using MessageCallback = std::function<void(const char* topic, const char* payload, size_t len)>;

    MQTTClient(const NetworkConfig::MQTTSettings& cfg,
               const char* root_ca,
               const char* device_cert,
               const char* private_key);

    // Install certs and configure the underlying clients. Idempotent.
    void begin();

    // Attempt a blocking MQTT connect; returns true on success. ~3-5s on
    // a healthy network. Logs the AWS-specific failure code from PubSubClient
    // if the broker rejects (auth/policy issue, wrong client id, etc.).
    bool connect();

    bool isConnected();
    void loop();
    void disconnect();

    bool publish(const char* topic, const char* payload);
    bool subscribe(const char* topic);

    // Set once; PubSubClient calls a fixed C function which forwards to this.
    void onMessage(MessageCallback cb) { cb_ = std::move(cb); }

private:
    // PubSubClient's setCallback signature: void(char*, uint8_t*, unsigned int).
    // We give it a free function that locates the active instance and forwards.
    static void onMqttMessage(char* topic, uint8_t* payload, unsigned int len);

    const NetworkConfig::MQTTSettings& cfg_;
    const char* root_ca_;
    const char* device_cert_;
    const char* private_key_;

    WiFiClientSecure tls_;
    PubSubClient     mqtt_;
    MessageCallback  cb_;
    bool             began_ = false;
};

#endif // MQTT_CLIENT_H
