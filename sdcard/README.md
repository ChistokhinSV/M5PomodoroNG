# SD Card Files for M5 Pomodoro v2

This directory contains example files to copy to your SD card.

## Quick Setup

1. **Format SD card** as FAT32 (recommended: 4GB-32GB)
2. **Copy entire contents** of this `sdcard/` folder to the root of your SD card
3. **Edit** `config/network.ini` with your WiFi credentials and AWS IoT settings
4. **(Optional)** Add SSL certificates to `config/certs/` (see setup guide below)
5. **(Optional)** Add custom audio files to `audio/` (see audio guide below)
6. **Insert SD card** into M5Stack Core2
7. **Power on** and check serial monitor

## Directory Structure

```
sdcard/                              (Copy all contents to SD card root)
├── README.md                        (This file - can delete from SD card)
├── config/
│   ├── network.ini                  (EDIT THIS - WiFi + AWS IoT settings)
│   ├── certs/
│   │   ├── README.md                (Certificate setup guide)
│   │   ├── AmazonRootCA1.pem        (ADD THIS - Download from Amazon)
│   │   ├── device-certificate.pem.crt  (ADD THIS - From AWS IoT Console)
│   │   └── device-private.pem.key  (ADD THIS - From AWS IoT Console)
│   └── lasttime.txt                 (AUTO-CREATED - Emergency time fallback)
└── audio/
    ├── README.md                    (Audio customization guide)
    ├── work_start.wav               (OPTIONAL - Custom work start sound)
    ├── rest_start.wav               (OPTIONAL - Custom break start sound)
    ├── long_rest_start.wav          (OPTIONAL - Custom long break sound)
    └── warning.wav                  (OPTIONAL - Custom 30-sec warning sound)
```

## Step-by-Step Setup Guide

### 1. Format SD Card

**Windows**:
1. Insert SD card into PC
2. Right-click SD card in File Explorer
3. Select "Format..."
4. File system: **FAT32** (or exFAT if >32GB)
5. Allocation size: **Default**
6. Click "Start"

**macOS**:
1. Open Disk Utility
2. Select SD card
3. Click "Erase"
4. Format: **MS-DOS (FAT)** or **exFAT**
5. Click "Erase"

**Linux**:
```bash
sudo mkfs.vfat -F 32 /dev/sdX1  # Replace sdX1 with your SD card device
```

### 2. Copy Files to SD Card

**Option A: Copy entire folder**
```bash
# Windows PowerShell
Copy-Item -Recurse sdcard\* E:\  # Replace E: with your SD card drive

# macOS/Linux
cp -r sdcard/* /Volumes/SDCARD/  # Replace path with your SD mount point
```

**Option B: Manual copy**
1. Open `sdcard/` folder
2. Select all files and folders (Ctrl+A / Cmd+A)
3. Copy to SD card root (not into a subfolder!)

### 3. Edit network.ini

**Required edits**:
- `[WiFi]` section:
  - `SSID=YourWiFiSSID` → Your WiFi network name
  - `Password=YourWiFiPassword` → Your WiFi password

**Optional edits**:
- `[NTP]` section: NTP server and timezone (named or fixed offset + DST)
- `[Webhook.N]` sections: HTTPS POST hooks for Telegram / Discord /
  Zapier / n8n / IFTTT / your own scripts (see below)
- `[MQTT]` + `[CloudSync]` sections: AWS IoT shadow sync (next firmware slice)

**Example**:
```ini
[WiFi]
SSID=HomeWiFi
Password=MySecurePassword123

[NTP]
Server=pool.ntp.org
# Pick ONE of the next two. TimezoneOffset wins if both are set.
# (a) Named zone — DST handled automatically:
Timezone=America/New_York
# (b) Fixed offset (seconds from UTC). Set DST=true to apply EU-style
#     auto-DST (last Sun Mar -> last Sun Oct) on top of the offset.
#TimezoneOffset=-18000
#DST=false
```

**Timezone options**:
- `Timezone=` accepts any IANA name (`Continent/City`) — the full
  ~470-entry IANA database is bundled via the
  [TzDbLookup](https://github.com/nayarsystems/posix_tz_db) library.
  Examples: `Europe/Belgrade`, `America/Los_Angeles`, `Asia/Tokyo`.
  A raw POSIX TZ string also works. Bare `UTC` and `GMT` are
  accepted as aliases. DST rules are baked in, so transitions
  happen automatically.
- `TimezoneOffset=` is the legacy fixed-offset path. Combine with
  `DST=true` for automatic summer-time shift using EU rules.

**Webhooks** (generic integration path):

On session events the device fires HTTPS POSTs with a JSON body to
each configured URL. Use this to plug into Telegram (via your own
bot or a relay), Discord, Zapier, n8n, IFTTT, or any homemade
script. Up to 4 endpoints in `[Webhook.1]` … `[Webhook.4]`.

```ini
[Webhook.1]
URL=https://discord.com/api/webhooks/<ID>/<TOKEN>
Events=work_complete,cycle_complete

[Webhook.2]
URL=https://hooks.zapier.com/hooks/catch/.../xyz
Events=*
AuthHeader=Bearer your_secret

# Telegram (no relay needed):
[Webhook.3]
URL=https://api.telegram.org/bot<TOKEN>/sendMessage
Format=telegram
ChatID=<CHAT_ID>
Events=work_complete,cycle_complete
Text=Pomodoro {event}: session {session_number}/{total_sessions}, today {today}
```

**Telegram setup**: message `@BotFather` on Telegram, `/newbot`, save the
token. Then message your new bot once, open
`https://api.telegram.org/bot<TOKEN>/getUpdates`, and copy the `chat.id`
from the JSON response. Personal chats are positive integers, groups are
negative (e.g. `-1001234567890`), public channels accept `@channelname`.

**Template placeholders** (used by `Format=telegram` `Text=`):
`{event}`, `{duration_min}`, `{session_number}`, `{total_sessions}`,
`{today}`, `{week}`, `{device}`. Unknown placeholders pass through
verbatim.

Supported event names: `work_complete`, `break_complete`,
`cycle_complete`. Use `*` (or omit `Events=`) to subscribe to all.

JSON payload:

```json
{
  "event": "work_complete",
  "device": "<MQTT.ClientID>",
  "timestamp": 1717612345,
  "duration_min": 25,
  "session_number": 3,
  "total_sessions": 4,
  "today": 5,
  "week": 18
}
```

WiFi is on-demand when **only** webhooks are configured: the device
connects when a session ends, fires the configured webhooks
sequentially, and disconnects. Battery cost is roughly one
~5-second WiFi burst per session boundary.

When the AWS IoT shadow path is enabled (next section), WiFi stays
**persistent** instead — needed for MQTT keepalive + delta delivery.

---

## AWS IoT Device Shadow

Set `[CloudSync] Enabled=true` and provide AWS IoT credentials to
have the device publish state to a Device Shadow and accept
server-side commands. Service-specific integrations (Toggl,
Google Calendar, etc.) live in a companion server-side app that
subscribes to the shadow — not in the firmware.

**Setup steps:**

1. **Create an AWS IoT Thing** (one-time per device). AWS Console >
   IoT Core > Manage > Things > Create things > Single thing.
   Give it a name (e.g. `M5StackCore2`).

2. **Create + attach a certificate.** During Thing creation, choose
   "Auto-generate a new certificate". Download all four files: the
   `*-certificate.pem.crt`, the `*-private.pem.key`, the
   `*-public.pem.key` (not needed at runtime), and
   `AmazonRootCA1.pem`. **The private key is shown ONCE** — save
   it now or you'll have to create a fresh cert.

3. **Attach an IoT policy** that allows the shadow operations.
   Minimum permissions:
   ```json
   {
     "Version": "2012-10-17",
     "Statement": [
       { "Effect": "Allow", "Action": "iot:Connect",
         "Resource": "arn:aws:iot:<region>:<account>:client/M5StackCore2" },
       { "Effect": "Allow",
         "Action": ["iot:Publish", "iot:Receive"],
         "Resource": "arn:aws:iot:<region>:<account>:topic/$aws/things/M5StackCore2/shadow/*" },
       { "Effect": "Allow", "Action": "iot:Subscribe",
         "Resource": "arn:aws:iot:<region>:<account>:topicfilter/$aws/things/M5StackCore2/shadow/*" }
     ]
   }
   ```

4. **Copy two files to the SD card** at `/config/certs/`:
   - `device-certificate.pem.crt` (rename the long hash filename)
   - `device-private.pem.key`
   `AmazonRootCA1.pem` is optional — the firmware has an embedded
   copy that's used automatically if SD doesn't provide one.

5. **Edit `/config/network.ini` `[MQTT]` and `[CloudSync]`:**
   ```ini
   [MQTT]
   Broker=<your>-ats.iot.<region>.amazonaws.com   ; from IoT Settings
   ClientID=M5StackCore2
   ThingName=M5StackCore2
   [CloudSync]
   Enabled=true
   ```

6. **Flash and watch serial.** You should see:
   ```
   [MQTT] Connecting to ...
   [MQTT] Connected as M5StackCore2 in NNNN ms
   [Shadow] Subscribe $aws/things/M5StackCore2/shadow/update/delta: OK
   [Shadow] Snapshot publish: OK
   ```

**Reported state** (server can read at any time):

```json
{ "state": { "reported": {
    "fw_version": "2.0.0",
    "device_state": "active",
    "session_type": "work",
    "session_number": 3, "total_sessions": 4,
    "duration_min": 25, "remaining_sec": 1234,
    "today": 5, "week": 18, "lifetime": 847,
    "last_event": "work_complete", "last_event_at": 1717612345
}}}
```

**Delta protocol** (server → device commands): set
`"state": { "command": "<verb>", "command_id": "<your-id>" }` in
the shadow's desired state. The firmware acts on the verb, then
publishes a reported state echoing the command + id so the delta
clears. Verbs: `start`, `pause`, `resume`, `skip`, `stop`.

### 4. (Optional) Setup SSL Certificates

**Only needed if you want cloud sync with AWS IoT**

See `config/certs/README.md` for complete setup guide:
1. Create AWS IoT Thing
2. Download 3 certificate files
3. Copy to `config/certs/` on SD card

**Quick links**:
- AWS Console: https://console.aws.amazon.com/iot/
- Amazon Root CA: https://www.amazontrust.com/repository/AmazonRootCA1.pem

### 5. (Optional) Add Custom Audio Files

**Only needed if you want custom sounds instead of embedded beeps**

See `audio/README.md` for audio file requirements:
- Format: WAV (16kHz, 16-bit, mono)
- Max size: ~100KB per file
- 4 sounds: work_start, rest_start, long_rest_start, warning

### 6. Insert SD Card and Test

1. **Power off** M5Stack Core2
2. **Insert SD card** (slot on right side of device)
3. **Power on** device
4. **Connect serial monitor** (115200 baud)
5. **Check for success messages**:
   ```
   [OK] SD card initialized: SDHC, X MB free
   [OK] Network configuration loaded from SD
   [OK] Audio player initialized
   [AudioPlayer] Audio source: SD_CARD
   ```

## Troubleshooting

### SD Card Not Detected

**Error**: `[WARN] SD card not available - using NVS/FLASH fallbacks`

**Solutions**:
1. Check SD card inserted properly (push until click)
2. Try different SD card (some cards incompatible)
3. Format as FAT32 (not exFAT or NTFS)
4. Check SD card size (4GB-32GB recommended)
5. Try slower SD card (some high-speed cards have issues)

### network.ini Not Found

**Error**: `[NetworkConfig] ERROR: /config/network.ini not found`

**Solutions**:
1. Verify files copied to **root** of SD card, not into subfolder
2. Check directory structure: SD card should have `config/` folder at root
3. Verify file name is exactly `network.ini` (not `network.ini.txt`)
4. Check file path: Should be `E:\config\network.ini` (Windows example)

### Audio Files Not Loading

**Error**: `[AudioPlayer] Using FLASH audio (embedded)`

**Solutions**:
1. Verify audio files in `/audio/` folder on SD card
2. Check file names exactly match: `work_start.wav`, `rest_start.wav`, etc.
3. Verify WAV format: 16kHz, 16-bit, mono
4. Check file sizes not too large (>100KB may fail to load)

### Invalid PEM Format

**Error**: `[NetworkConfig] ERROR: Invalid PEM format for device cert`

**Solutions**:
1. Open certificate file in text editor
2. Verify starts with `-----BEGIN CERTIFICATE-----`
3. Check not saved as HTML (common browser download issue)
4. Re-download from AWS IoT Console if corrupted

## File Formats Reference

### network.ini Format
- **Type**: INI configuration file
- **Encoding**: UTF-8 or ASCII
- **Line endings**: Any (LF, CRLF, CR)
- **Comments**: Lines starting with `#` or `;`
- **Sections**: `[SectionName]`
- **Key-value**: `Key=Value`

### Certificate Files (.pem, .crt, .key)
- **Type**: PEM (Privacy-Enhanced Mail) format
- **Encoding**: Base64-encoded DER
- **Structure**: Header + Base64 data + Footer
- **Example**:
  ```
  -----BEGIN CERTIFICATE-----
  MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF
  ...
  -----END CERTIFICATE-----
  ```

### Audio Files (.wav)
- **Type**: WAV (Waveform Audio File Format)
- **Sample rate**: 16000 Hz (16kHz)
- **Bit depth**: 16-bit signed PCM
- **Channels**: 1 (mono)
- **Max size**: ~100KB (recommended)
- **Duration**: 1-3 seconds typical

## Security Best Practices

1. **Never commit `network.ini` or certificates to Git**
   - The project `.gitignore` excludes these automatically
   - Double-check before pushing!

2. **Keep private key secure**
   - `device-private.pem.key` is like a password
   - Don't share, email, or post online
   - If compromised, revoke certificate in AWS IoT Console

3. **Use strong WiFi password**
   - Stored in plain text on SD card
   - Physical access to SD card = WiFi access

4. **Rotate certificates annually**
   - AWS IoT certificates don't expire by default
   - Best practice: Create new certificate yearly
   - Revoke old certificate in AWS Console

5. **Remove SD card when not in use**
   - Device stores credentials in NVS (internal flash)
   - Can operate offline after first WiFi connection
   - Remove SD card to prevent theft of credentials

## Additional Resources

- **M5Stack Core2 Docs**: https://docs.m5stack.com/en/core/core2
- **AWS IoT Core Docs**: https://docs.aws.amazon.com/iot/
- **PlatformIO Docs**: https://docs.platformio.org/
- **Project GitHub**: (Add your repo URL here)

---

**Last Updated**: 2025-01-10
**Version**: 2.0.0
**Related**: MP-70 (SDManager), MP-73 (network.ini), MP-74 (SSL certificates)
