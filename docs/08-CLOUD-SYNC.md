# Cloud Synchronization Specification

## 1. Overview

This document specifies the cloud synchronization architecture for M5 Pomodoro Timer v2, including AWS IoT Device Shadow integration, Toggl time tracking, and Google Calendar event creation. The system maintains eventual consistency between device state and cloud services while optimizing for battery life and offline operation.

### Design Principles

1. **Offline-First**: Device operates independently, syncs when WiFi available
2. **Eventual Consistency**: Cloud state eventually reflects device state (no strong consistency guarantee)
3. **Battery Optimization**: WiFi duty cycling based on power mode
4. **Conflict Resolution**: Last-write-wins (LWW) with device as source of truth
5. **Graceful Degradation**: Full functionality without cloud connectivity

### Cloud Services

```
M5Stack Core2
    ↓
    ├─→ AWS IoT Core (MQTT over TLS)
    │   └─→ Device Shadow (state sync)
    │       └─→ Lambda Functions
    │           ├─→ Toggl API (time tracking)
    │           └─→ Google Calendar API (event creation)
    │
    └─→ NTP Server (time synchronization)
```

## 2. AWS IoT Device Shadow

### Thing Configuration

```json
{
  "thingName": "m5-pomodoro-<DEVICE_ID>",
  "thingTypeName": "M5PomodoroTimer",
  "attributes": {
    "firmwareVersion": "2.0.0",
    "hardwareModel": "M5Stack Core2",
    "serialNumber": "<MAC_ADDRESS>"
  }
}
```

**Thing Name**: `m5-pomodoro-<DEVICE_ID>`
- `<DEVICE_ID>`: Last 6 hex digits of MAC address (e.g., `m5-pomodoro-A1B2C3`)
- Unique per device, immutable

### Shadow Document Schema

```json
{
  "state": {
    "reported": {
      "timer": {
        "state": "POMODORO",
        "remainingSeconds": 1234,
        "sessionNumber": 1,
        "sequencePosition": 0,
        "startedAt": 1703001234,
        "mode": "CLASSIC"
      },
      "settings": {
        "pomodoroDuration": 25,
        "shortRestDuration": 5,
        "longRestDuration": 15,
        "studyMode": false,
        "powerMode": "BALANCED",
        "gyroSensitivity": 60,
        "gyroWake": true
      },
      "stats": {
        "todayCompleted": 6,
        "todayStarted": 7,
        "todayWorkMinutes": 150,
        "currentStreak": 3,
        "bestStreak": 12,
        "lifetimePomodoros": 234
      },
      "device": {
        "batteryPercent": 85,
        "firmwareVersion": "2.0.0",
        "uptimeSeconds": 3600,
        "freeMemoryKB": 485,
        "wifiRSSI": -45
      },
      "lastSync": 1703001234
    },
    "desired": {
      "settings": {
        "pomodoroDuration": 25,
        "shortRestDuration": 5,
        "longRestDuration": 15,
        "studyMode": false
      }
    }
  },
  "metadata": {
    "reported": {
      "timer": {
        "state": {
          "timestamp": 1703001234
        }
      }
    }
  },
  "version": 42,
  "timestamp": 1703001234
}
```

### Shadow Update Strategy

**When to Update (Reported State)**:

1. **Timer State Changes**:
   - START event: Immediately (within 5s)
   - PAUSE/RESUME: Immediately
   - STOP: Immediately
   - EXPIRE: Immediately + statistics update
   - State transitions: Immediately

2. **Statistics Updates**:
   - Session completion: Immediately
   - Midnight rollover: Next sync (non-critical)
   - Streak changes: Next sync

3. **Settings Changes**:
   - User modifies settings: Within 30s

4. **Periodic Heartbeat**:
   - Every 5 minutes (PERFORMANCE mode)
   - Every 10 minutes (BALANCED mode)
   - Every 30 minutes (BATTERY_SAVER mode)
   - Updates: battery, uptime, memory, RSSI

**When to Accept (Desired State)**:

1. **Remote Settings Changes**:
   - Apply desired settings to device
   - Update reported state to match
   - Show toast: "Settings updated from cloud"

2. **Conflict Resolution**:
   - If device settings changed locally AND desired changed: Device wins
   - Clear desired state after applying

### Shadow Delta Handling

```cpp
void CloudSync::handleShadowDelta(const JsonDocument& delta) {
    // Extract desired state changes
    if (delta.containsKey("settings")) {
        JsonObject desiredSettings = delta["settings"];

        // Only apply if device hasn't changed locally in last 10s
        if (millis() - lastLocalSettingsChange > 10000) {
            applyRemoteSettings(desiredSettings);

            // Clear desired state
            publishShadowUpdate("{\"state\":{\"desired\":null}}");
        } else {
            LOG_INFO("Ignoring remote settings: local changes take precedence");
        }
    }
}
```

## 3. MQTT Topics

### Subscribe Topics

```cpp
const char* SHADOW_UPDATE_ACCEPTED = "$aws/things/m5-pomodoro-<DEVICE_ID>/shadow/update/accepted";
const char* SHADOW_UPDATE_REJECTED = "$aws/things/m5-pomodoro-<DEVICE_ID>/shadow/update/rejected";
const char* SHADOW_UPDATE_DELTA = "$aws/things/m5-pomodoro-<DEVICE_ID>/shadow/update/delta";
const char* SHADOW_GET_ACCEPTED = "$aws/things/m5-pomodoro-<DEVICE_ID>/shadow/get/accepted";
const char* SHADOW_GET_REJECTED = "$aws/things/m5-pomodoro-<DEVICE_ID>/shadow/get/rejected";
```

### Publish Topics

```cpp
const char* SHADOW_UPDATE = "$aws/things/m5-pomodoro-<DEVICE_ID>/shadow/update";
const char* SHADOW_GET = "$aws/things/m5-pomodoro-<DEVICE_ID>/shadow/get";
const char* SHADOW_DELETE = "$aws/things/m5-pomodoro-<DEVICE_ID>/shadow/delete";
```

### QoS Levels

```
QoS 0 (At most once): Periodic heartbeats, non-critical stats
QoS 1 (At least once): Timer state changes, session completions, settings
```

**Rationale**: QoS 1 ensures critical state changes are delivered, but allows duplicate messages (handled by idempotent processing). QoS 2 avoided due to battery cost.

## 4. Connection Management

### WiFi Duty Cycling

**PERFORMANCE Mode**:
```cpp
WiFi.mode(WIFI_STA);
WiFi.setAutoReconnect(true);
// Always connected
// MQTT keepalive: 60s
```

**BALANCED Mode** (Target):
```cpp
// Connect every 120 seconds
void balancedWiFiCycle() {
    static uint32_t lastConnect = 0;

    if (millis() - lastConnect > 120000) {
        WiFi.begin();
        waitForConnection(15000);  // 15s timeout

        if (WiFi.isConnected()) {
            mqttClient.connect();
            syncShadow();  // ~2-5s
            mqttClient.disconnect();
        }

        WiFi.disconnect(true);  // Turn off WiFi radio
        lastConnect = millis();
    }
}
```

**BATTERY_SAVER Mode**:
```cpp
// On-demand only (user-triggered or session completion)
void batterySaverWiFiCycle() {
    // Only connect when:
    // 1. User taps "Sync" button in Settings
    // 2. Session completed (to log to Toggl)
    // 3. Manual sync requested
}
```

### MQTT Connection Lifecycle

```cpp
class CloudSync {
    void connect();
    void disconnect();
    void reconnect();
    void handleDisconnect();
};

void CloudSync::connect() {
    // Step 1: Ensure WiFi connected
    if (!WiFi.isConnected()) {
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        if (!waitForWiFi(15000)) {
            LOG_ERROR("WiFi connection failed");
            return;
        }
    }

    // Step 2: Load AWS IoT credentials from NVS
    loadAwsCredentials();

    // Step 3: Set up TLS
    wifiClientSecure.setCACert(AWS_CERT_CA);
    wifiClientSecure.setCertificate(AWS_CERT_CRT);
    wifiClientSecure.setPrivateKey(AWS_CERT_PRIVATE);

    // Step 4: Connect MQTT
    mqttClient.setServer(AWS_IOT_ENDPOINT, 8883);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setKeepAlive(60);  // PERFORMANCE
    // mqttClient.setKeepAlive(300);  // BALANCED (5 min)

    String clientId = "m5-pomodoro-" + getDeviceId();

    if (mqttClient.connect(clientId.c_str())) {
        LOG_INFO("MQTT connected");
        subscribeToShadowTopics();
        requestShadowState();  // Sync on connect
    } else {
        LOG_ERROR("MQTT connection failed: " + String(mqttClient.state()));
        handleConnectionError();
    }
}

void CloudSync::disconnect() {
    if (mqttClient.connected()) {
        mqttClient.disconnect();
    }

    if (WiFi.isConnected()) {
        WiFi.disconnect(true);  // true = turn off radio
    }

    LOG_INFO("Cloud disconnected");
}
```

### Reconnection Strategy

```cpp
void CloudSync::reconnect() {
    static uint32_t lastAttempt = 0;
    static uint8_t retryCount = 0;

    // Exponential backoff
    uint32_t backoff[] = {1000, 2000, 5000, 10000, 30000, 60000};  // ms
    uint32_t delay = backoff[min(retryCount, 5)];

    if (millis() - lastAttempt < delay) {
        return;  // Too soon to retry
    }

    LOG_INFO("Reconnecting... (attempt " + String(retryCount + 1) + ")");
    lastAttempt = millis();

    if (connect()) {
        retryCount = 0;  // Reset on success
    } else {
        retryCount++;
        if (retryCount >= 6) {
            // After 6 failures (~110s total), give up until next scheduled sync
            LOG_ERROR("Reconnection failed, waiting for next sync window");
            retryCount = 0;
        }
    }
}
```

## 5. Synchronization Protocol

### Full Sync Sequence

```
Device                          AWS IoT Core
  │                                  │
  ├─ Connect WiFi                    │
  ├─ Connect MQTT/TLS ──────────────>│
  │                                  │
  ├─ Subscribe to shadow topics      │
  │  ├─ .../update/accepted          │
  │  ├─ .../update/rejected          │
  │  ├─ .../update/delta             │
  │  └─ .../get/accepted             │
  │                                  │
  ├─ Publish to .../shadow/get ─────>│
  │                                  │
  │<───── .../get/accepted ──────────┤ (full shadow)
  │                                  │
  ├─ Merge shadow with local state   │
  │                                  │
  ├─ Publish to .../shadow/update ──>│ (reported state)
  │                                  │
  │<───── .../update/accepted ───────┤
  │                                  │
  ├─ Check for delta                 │
  │<───── .../update/delta ──────────┤ (if desired ≠ reported)
  │                                  │
  ├─ Apply desired settings          │
  ├─ Clear desired ─────────────────>│
  │                                  │
  └─ Disconnect (BALANCED mode)      │
```

### Incremental Update

```cpp
void CloudSync::publishTimerUpdate(PomodoroState state, uint16_t remaining) {
    StaticJsonDocument<256> doc;
    JsonObject reported = doc.createNestedObject("state").createNestedObject("reported");

    JsonObject timer = reported.createNestedObject("timer");
    timer["state"] = stateToString(state);
    timer["remainingSeconds"] = remaining;
    timer["sessionNumber"] = getSessionNumber();
    timer["sequencePosition"] = getSequencePosition();
    timer["startedAt"] = getCurrentEpoch();

    reported["lastSync"] = getCurrentEpoch();

    String payload;
    serializeJson(doc, payload);

    mqttClient.publish(SHADOW_UPDATE, payload.c_str(), false);  // QoS 1
}
```

### Statistics Sync

```cpp
void CloudSync::publishStatisticsUpdate() {
    StaticJsonDocument<384> doc;
    JsonObject reported = doc.createNestedObject("state").createNestedObject("reported");

    JsonObject stats = reported.createNestedObject("stats");
    Statistics s = getStatistics();

    stats["todayCompleted"] = s.getTodayCompleted();
    stats["todayStarted"] = s.getTodayStarted();
    stats["todayWorkMinutes"] = s.getTodayWorkMinutes();
    stats["currentStreak"] = s.getCurrentStreak();
    stats["bestStreak"] = s.getBestStreak();
    stats["lifetimePomodoros"] = s.getLifetimePomodoros();

    reported["lastSync"] = getCurrentEpoch();

    String payload;
    serializeJson(doc, payload);

    mqttClient.publish(SHADOW_UPDATE, payload.c_str(), false);  // QoS 1
}
```

## 6. Toggl Integration

### Architecture

```
M5Stack Core2
    ↓ (session completion)
    ↓
AWS IoT Core
    ↓ (IoT Rule)
    ↓
Lambda: LogPomodoroToToggl
    ↓ (HTTP POST)
    ↓
Toggl API v9
```

### IoT Rule

```sql
SELECT
    state.reported.timer.state AS timerState,
    state.reported.timer.sessionNumber AS sessionNumber,
    state.reported.timer.startedAt AS startedAt,
    state.reported.settings.pomodoroDuration AS duration,
    timestamp() AS receivedAt
FROM
    '$aws/things/+/shadow/update/accepted'
WHERE
    state.reported.timer.state = 'SHORT_REST' OR
    state.reported.timer.state = 'LONG_REST'
```

**Action**: Invoke Lambda function `LogPomodoroToToggl`

### Lambda Function

```javascript
// LogPomodoroToToggl
const axios = require('axios');

exports.handler = async (event) => {
    const { timerState, sessionNumber, startedAt, duration } = event;

    // Calculate end time
    const startTime = new Date(startedAt * 1000);
    const endTime = new Date(startTime.getTime() + duration * 60 * 1000);

    // Toggl API request
    const togglEntry = {
        description: `Pomodoro #${sessionNumber}`,
        start: startTime.toISOString(),
        stop: endTime.toISOString(),
        duration: duration * 60,
        created_with: "M5 Pomodoro Timer v2",
        tags: ["pomodoro", "m5stack"]
    };

    try {
        const response = await axios.post(
            'https://api.track.toggl.com/api/v9/time_entries',
            togglEntry,
            {
                headers: {
                    'Content-Type': 'application/json',
                    'Authorization': `Basic ${Buffer.from(process.env.TOGGL_API_TOKEN + ':api_token').toString('base64')}`
                }
            }
        );

        console.log('Toggl entry created:', response.data.id);
        return { statusCode: 200, body: JSON.stringify(response.data) };
    } catch (error) {
        console.error('Toggl API error:', error.message);
        return { statusCode: 500, body: error.message };
    }
};
```

### Device-Side Configuration

```cpp
struct TogglConfig {
    bool enabled;
    char apiToken[128];      // Stored in NVS (encrypted)
    uint32_t workspaceId;
    uint32_t projectId;      // Optional
    char tags[256];          // Comma-separated
};

void CloudSync::configureToggl(const TogglConfig& config) {
    // Store API token in NVS (encrypted)
    nvs_handle_t handle;
    nvs_open("toggl", NVS_READWRITE, &handle);
    nvs_set_blob(handle, "api_token", config.apiToken, strlen(config.apiToken));
    nvs_set_u8(handle, "enabled", config.enabled ? 1 : 0);
    nvs_commit(handle);
    nvs_close(handle);

    // Publish to shadow (without token, just enabled status)
    publishTogglStatus(config.enabled);
}
```

**Security Note**: API token never sent to shadow, only stored locally in NVS. Lambda uses token stored in AWS Secrets Manager.

## 7. Google Calendar Integration

### Architecture

```
M5Stack Core2
    ↓ (session completion)
    ↓
AWS IoT Core
    ↓ (IoT Rule)
    ↓
Lambda: CreateCalendarEvent
    ↓ (HTTP POST)
    ↓
Google Calendar API
```

### Lambda Function

```javascript
// CreateCalendarEvent
const { google } = require('googleapis');

exports.handler = async (event) => {
    const { timerState, sessionNumber, startedAt, duration } = event;

    // OAuth2 client (refresh token stored in Secrets Manager)
    const oauth2Client = new google.auth.OAuth2(
        process.env.GOOGLE_CLIENT_ID,
        process.env.GOOGLE_CLIENT_SECRET
    );

    oauth2Client.setCredentials({
        refresh_token: process.env.GOOGLE_REFRESH_TOKEN
    });

    const calendar = google.calendar({ version: 'v3', auth: oauth2Client });

    const startTime = new Date(startedAt * 1000);
    const endTime = new Date(startTime.getTime() + duration * 60 * 1000);

    const event = {
        summary: `Pomodoro #${sessionNumber}`,
        description: 'Created by M5 Pomodoro Timer v2',
        start: {
            dateTime: startTime.toISOString(),
            timeZone: 'UTC'
        },
        end: {
            dateTime: endTime.toISOString(),
            timeZone: 'UTC'
        },
        colorId: '11',  // Red for Pomodoro
        reminders: {
            useDefault: false
        }
    };

    try {
        const response = await calendar.events.insert({
            calendarId: 'primary',
            resource: event
        });

        console.log('Calendar event created:', response.data.id);
        return { statusCode: 200, body: JSON.stringify(response.data) };
    } catch (error) {
        console.error('Google Calendar API error:', error.message);
        return { statusCode: 500, body: error.message };
    }
};
```

### Device-Side Configuration

```cpp
struct CalendarConfig {
    bool enabled;
    char calendarId[128];    // "primary" or specific calendar ID
    uint8_t colorId;         // 1-11 (Google Calendar colors)
};

void CloudSync::configureCalendar(const CalendarConfig& config) {
    nvs_handle_t handle;
    nvs_open("calendar", NVS_READWRITE, &handle);
    nvs_set_u8(handle, "enabled", config.enabled ? 1 : 0);
    nvs_set_str(handle, "calendar_id", config.calendarId);
    nvs_set_u8(handle, "color_id", config.colorId);
    nvs_commit(handle);
    nvs_close(handle);

    publishCalendarStatus(config.enabled);
}
```

## 8. NTP Time Synchronization

### Purpose

Device requires accurate time for:
1. Session timestamps (startedAt epoch)
2. Statistics date tracking (YYYYMMDD)
3. Midnight rollover detection
4. Cloud sync timestamps

### Implementation

```cpp
class NTPClient {
public:
    void begin();
    bool sync(uint16_t timeoutMs = 5000);
    uint32_t getEpoch();
    bool isSynced();

private:
    bool synced = false;
    uint32_t lastSyncMillis = 0;
    uint32_t syncInterval = 3600000;  // 1 hour
};

void NTPClient::begin() {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}

bool NTPClient::sync(uint16_t timeoutMs) {
    struct tm timeinfo;
    uint32_t startTime = millis();

    while (!getLocalTime(&timeinfo)) {
        if (millis() - startTime > timeoutMs) {
            LOG_ERROR("NTP sync timeout");
            return false;
        }
        delay(100);
    }

    synced = true;
    lastSyncMillis = millis();
    LOG_INFO("NTP synced: " + String(asctime(&timeinfo)));
    return true;
}

uint32_t NTPClient::getEpoch() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        LOG_ERROR("Failed to obtain time");
        return 0;
    }
    return mktime(&timeinfo);
}
```

### Sync Strategy

1. **On WiFi Connect**: Sync NTP (5s timeout)
2. **Every Hour**: Re-sync if WiFi connected
3. **Drift Correction**: If correction >60s during active timer, pause and confirm

```cpp
void CloudSync::handleNTPDrift() {
    uint32_t oldEpoch = ntpClient.getEpoch();
    ntpClient.sync();
    uint32_t newEpoch = ntpClient.getEpoch();

    int32_t drift = newEpoch - oldEpoch;

    if (abs(drift) > 60 && timerStateMachine.isRunning()) {
        // Significant drift during active session
        timerStateMachine.pause();
        showToast("Time adjusted by NTP", ToastType::INFO);

        // Show confirmation dialog
        showDialog("System time changed. Continue timer?",
                   []() { timerStateMachine.resume(); },  // YES
                   []() { timerStateMachine.stop(); });   // NO
    }
}
```

## 9. Offline Queue

### Purpose

When WiFi unavailable, queue critical events for later sync.

### Data Structure

```cpp
struct QueuedEvent {
    uint8_t eventType;       // TIMER_START=1, TIMER_STOP=2, SESSION_COMPLETE=3
    uint32_t timestamp;      // Epoch when event occurred
    uint8_t sessionNumber;
    uint16_t duration;
    char payload[256];       // JSON payload
};

class OfflineQueue {
public:
    void enqueue(const QueuedEvent& event);
    QueuedEvent dequeue();
    bool isEmpty();
    uint8_t size();
    void clear();

private:
    QueuedEvent queue[32];   // Max 32 queued events (~8KB)
    uint8_t head = 0;
    uint8_t tail = 0;
};
```

### Storage

```cpp
void OfflineQueue::persist() {
    nvs_handle_t handle;
    nvs_open("offline_queue", NVS_READWRITE, &handle);

    // Store queue size
    nvs_set_u8(handle, "size", size());

    // Store each event
    for (uint8_t i = 0; i < size(); i++) {
        char key[16];
        snprintf(key, sizeof(key), "event_%d", i);
        nvs_set_blob(handle, key, &queue[(head + i) % 32], sizeof(QueuedEvent));
    }

    nvs_commit(handle);
    nvs_close(handle);
}

void OfflineQueue::restore() {
    nvs_handle_t handle;
    nvs_open("offline_queue", NVS_READONLY, &handle);

    uint8_t queueSize = 0;
    nvs_get_u8(handle, "size", &queueSize);

    for (uint8_t i = 0; i < queueSize; i++) {
        char key[16];
        snprintf(key, sizeof(key), "event_%d", i);
        size_t length = sizeof(QueuedEvent);
        nvs_get_blob(handle, key, &queue[i], &length);
    }

    head = 0;
    tail = queueSize;

    nvs_close(handle);
}
```

### Sync on Reconnect

```cpp
void CloudSync::processOfflineQueue() {
    while (!offlineQueue.isEmpty()) {
        QueuedEvent event = offlineQueue.dequeue();

        // Publish event to shadow
        mqttClient.publish(SHADOW_UPDATE, event.payload, false);  // QoS 1

        // Wait for acknowledgment (with timeout)
        if (waitForPublishAck(5000)) {
            LOG_INFO("Queued event synced: " + String(event.eventType));
        } else {
            // Re-queue if publish failed
            offlineQueue.enqueue(event);
            LOG_ERROR("Failed to sync queued event, re-queuing");
            break;
        }
    }

    offlineQueue.persist();  // Save updated queue to NVS
}
```

## 10. Error Handling

### Connection Errors

```cpp
void CloudSync::handleConnectionError() {
    switch (mqttClient.state()) {
        case MQTT_CONNECTION_TIMEOUT:
            LOG_ERROR("MQTT connection timeout");
            showToast("Cloud sync timeout", ToastType::WARNING);
            break;

        case MQTT_CONNECTION_LOST:
            LOG_ERROR("MQTT connection lost");
            reconnect();  // Auto-reconnect
            break;

        case MQTT_CONNECT_FAILED:
            LOG_ERROR("MQTT connect failed");
            showToast("Cloud connect failed", ToastType::ERROR);
            break;

        case MQTT_DISCONNECTED:
            LOG_INFO("MQTT disconnected (expected)");
            break;

        case MQTT_CONNECT_BAD_PROTOCOL:
        case MQTT_CONNECT_BAD_CLIENT_ID:
        case MQTT_CONNECT_UNAVAILABLE:
        case MQTT_CONNECT_BAD_CREDENTIALS:
        case MQTT_CONNECT_UNAUTHORIZED:
            LOG_ERROR("MQTT auth error: " + String(mqttClient.state()));
            showToast("Cloud auth failed", ToastType::ERROR);
            // Don't retry auth errors, require user intervention
            break;
    }
}
```

### Shadow Update Rejection

```cpp
void CloudSync::handleShadowRejected(const JsonDocument& error) {
    uint16_t errorCode = error["code"];
    const char* errorMessage = error["message"];

    LOG_ERROR("Shadow update rejected: " + String(errorCode) + " - " + String(errorMessage));

    switch (errorCode) {
        case 400:  // Bad request
            LOG_ERROR("Malformed shadow document");
            break;

        case 401:  // Unauthorized
            showToast("Cloud auth failed", ToastType::ERROR);
            break;

        case 409:  // Conflict (version mismatch)
            LOG_WARN("Shadow version conflict, requesting latest state");
            requestShadowState();  // Re-sync
            break;

        case 413:  // Payload too large
            LOG_ERROR("Shadow document too large");
            break;

        case 429:  // Too many requests
            LOG_WARN("Rate limited, backing off");
            delay(5000);
            break;

        default:
            LOG_ERROR("Unknown error: " + String(errorCode));
            break;
    }
}
```

### Timeout Handling

```cpp
bool CloudSync::waitForPublishAck(uint16_t timeoutMs) {
    uint32_t startTime = millis();

    while (!publishAcknowledged && (millis() - startTime < timeoutMs)) {
        mqttClient.loop();  // Process incoming messages
        delay(10);
    }

    if (publishAcknowledged) {
        publishAcknowledged = false;
        return true;
    } else {
        LOG_ERROR("Publish acknowledgment timeout");
        return false;
    }
}
```

## 11. Security

### TLS Configuration

```cpp
void CloudSync::setupTLS() {
    // Load certificates from NVS or embedded in firmware
    const char* AWS_CERT_CA = loadCertificate("aws_ca");
    const char* AWS_CERT_CRT = loadCertificate("aws_crt");
    const char* AWS_CERT_PRIVATE = loadCertificate("aws_key");

    wifiClientSecure.setCACert(AWS_CERT_CA);
    wifiClientSecure.setCertificate(AWS_CERT_CRT);
    wifiClientSecure.setPrivateKey(AWS_CERT_PRIVATE);

    // Optional: Enable certificate validation
    wifiClientSecure.setInsecure(false);  // Validate server cert
}
```

### Certificate Storage

**Option 1: Embedded in Firmware** (compile-time)
```cpp
// certs.h
const char AWS_CERT_CA[] = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF
...
-----END CERTIFICATE-----
)EOF";
```

**Option 2: Provisioned to NVS** (runtime, requires provisioning tool)
```cpp
void provisionCertificates(const char* ca, const char* crt, const char* key) {
    nvs_handle_t handle;
    nvs_open("certs", NVS_READWRITE, &handle);
    nvs_set_blob(handle, "aws_ca", ca, strlen(ca));
    nvs_set_blob(handle, "aws_crt", crt, strlen(crt));
    nvs_set_blob(handle, "aws_key", key, strlen(key));
    nvs_commit(handle);
    nvs_close(handle);
}
```

**Recommendation**: Use embedded certificates for v2.0.0, add provisioning in future version.

### API Token Encryption

```cpp
// Encrypt Toggl API token before storing in NVS
void CloudSync::encryptAndStore(const char* key, const char* plaintext) {
    // Use mbedtls AES-256-GCM
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);

    // Derive encryption key from device-specific secret
    unsigned char encKey[32];
    deriveDeviceKey(encKey);

    // Encrypt
    unsigned char ciphertext[256];
    unsigned char tag[16];
    mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, strlen(plaintext),
                              iv, sizeof(iv), NULL, 0,
                              (unsigned char*)plaintext, ciphertext, 16, tag);

    // Store encrypted data
    nvs_set_blob(handle, key, ciphertext, strlen(plaintext));
    nvs_set_blob(handle, "tag", tag, 16);
}
```

**Note**: Full encryption implementation deferred to MP-36 (Cloud sync implementation).

## 12. Testing Strategy

### Unit Tests

```cpp
TEST(CloudSyncTest, ShadowDocumentSerialization) {
    TimerState state = {
        .state = PomodoroState::POMODORO,
        .remainingSeconds = 1234,
        .sessionNumber = 1,
        .sequencePosition = 0,
        .startedAt = 1703001234
    };

    CloudSync cloud;
    String json = cloud.serializeShadowUpdate(state);

    // Verify JSON structure
    StaticJsonDocument<512> doc;
    deserializeJson(doc, json);

    ASSERT_EQ(doc["state"]["reported"]["timer"]["state"], "POMODORO");
    ASSERT_EQ(doc["state"]["reported"]["timer"]["remainingSeconds"], 1234);
}

TEST(CloudSyncTest, OfflineQueuePersistence) {
    OfflineQueue queue;
    QueuedEvent event = {
        .eventType = SESSION_COMPLETE,
        .timestamp = 1703001234,
        .sessionNumber = 1,
        .duration = 25
    };

    queue.enqueue(event);
    queue.persist();

    OfflineQueue restoredQueue;
    restoredQueue.restore();

    ASSERT_EQ(restoredQueue.size(), 1);
    QueuedEvent restored = restoredQueue.dequeue();
    ASSERT_EQ(restored.eventType, SESSION_COMPLETE);
}
```

### Integration Tests

```cpp
TEST(CloudSyncIntegrationTest, FullSyncCycle) {
    CloudSync cloud;

    // Mock MQTT client
    MockMQTTClient mockClient;
    cloud.setMQTTClient(&mockClient);

    // Connect
    ASSERT_TRUE(cloud.connect());
    ASSERT_TRUE(mockClient.isConnected());

    // Publish timer update
    cloud.publishTimerUpdate(PomodoroState::POMODORO, 1500);

    // Verify MQTT publish was called
    ASSERT_EQ(mockClient.getPublishCount(), 1);
    ASSERT_EQ(mockClient.getLastTopic(), "$aws/things/m5-pomodoro-TEST/shadow/update");

    // Simulate shadow delta
    const char* delta = R"({"state":{"desired":{"settings":{"pomodoroDuration":30}}}})";
    mockClient.simulateMessage("$aws/things/m5-pomodoro-TEST/shadow/update/delta", delta);

    // Verify settings were updated
    ASSERT_EQ(getSettings().pomodoroDuration, 30);
}
```

### Manual Testing Checklist

- [ ] WiFi connection in all power modes
- [ ] MQTT connection with valid credentials
- [ ] Shadow update on timer state change
- [ ] Shadow delta handling (remote settings change)
- [ ] Offline queue persistence across reboot
- [ ] NTP sync and drift detection
- [ ] Toggl time entry creation (verify in Toggl web UI)
- [ ] Google Calendar event creation (verify in Calendar)
- [ ] TLS certificate validation
- [ ] Graceful handling of WiFi disconnect during sync
- [ ] Battery consumption in BALANCED mode (target: <10mA average)

## 13. Performance Metrics

### Sync Duration

```
Full sync (cold start):     8-12 seconds
    ├─ WiFi connect:        3-5s
    ├─ MQTT connect:        2-3s
    ├─ Shadow GET:          1-2s
    ├─ Shadow UPDATE:       1-2s
    └─ NTP sync:            1-2s

Incremental update:         2-3 seconds
    ├─ WiFi connect:        0s (already connected)
    ├─ MQTT publish:        1-2s
    └─ Acknowledgment:      1s
```

### Battery Impact

```
PERFORMANCE mode (always connected):
    WiFi idle:      ~80mA
    MQTT keepalive: +5mA (60s interval)
    Total:          ~85mA

BALANCED mode (periodic sync every 120s):
    Average:        ~10mA
    Peak (during sync): ~85mA for 5s
    Idle (WiFi off): ~5mA

BATTERY_SAVER mode (on-demand only):
    Average:        ~5mA (WiFi off)
    Sync event:     ~85mA for 8s
```

**Target**: <10mA average in BALANCED mode (4+ hour battery life)

### Network Usage

```
Shadow document size:   ~800 bytes
QoS 1 overhead:         +50 bytes (MQTT headers)
TLS overhead:           +5% (encryption)
Total per sync:         ~900 bytes

Daily usage (BALANCED mode):
    Syncs per day:      ~120 (every 120s × 14400s / 120s)
    Data per day:       ~108 KB (120 × 900 bytes)
    Monthly:            ~3.2 MB
```

## 14. Future Enhancements

### MQTT over WebSocket

For networks that block port 8883:

```cpp
void CloudSync::connectWebSocket() {
    // wss://xxxxxxxx.iot.us-east-1.amazonaws.com/mqtt
    wifiClientSecure.connect(AWS_IOT_ENDPOINT, 443);
    mqttClient.setServer(wifiClientSecure, AWS_IOT_ENDPOINT);
}
```

### Delta Compression

For large shadow documents:

```cpp
void CloudSync::publishDelta(const JsonDocument& changes) {
    // Only send changed fields, not full document
    StaticJsonDocument<256> doc;
    doc["state"]["reported"] = changes;
    mqttClient.publish(SHADOW_UPDATE, doc.as<String>());
}
```

### OTA Firmware Updates

Use AWS IoT Jobs for firmware distribution:

```cpp
void CloudSync::handleOTAJob(const JsonDocument& job) {
    const char* firmwareUrl = job["document"]["firmwareUrl"];
    downloadAndInstallFirmware(firmwareUrl);
}
```

### Multi-Device Sync

Sync statistics across multiple M5Stack devices:

```cpp
// Use DynamoDB table for shared statistics
// Shadow only contains device-specific state
// Lambda aggregates stats from all devices
```

## 15. Related Documents

- [02-ARCHITECTURE.md](02-ARCHITECTURE.md) - Network task architecture (Core 1)
- [05-STATISTICS-SYSTEM.md](05-STATISTICS-SYSTEM.md) - Statistics data to sync
- [06-POWER-MANAGEMENT.md](06-POWER-MANAGEMENT.md) - WiFi duty cycling for battery life

## 16. References

- AWS IoT Device Shadow: https://docs.aws.amazon.com/iot/latest/developerguide/iot-device-shadows.html
- MQTT over TLS: https://docs.aws.amazon.com/iot/latest/developerguide/protocols.html
- Toggl API v9: https://developers.track.toggl.com/docs/
- Google Calendar API: https://developers.google.com/calendar/api/v3/reference
- ESP32 WiFiClientSecure: https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFiClientSecure/src/WiFiClientSecure.h
- PubSubClient MQTT: https://github.com/knolleary/pubsubclient
