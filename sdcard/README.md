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
- `[MQTT]` section: AWS IoT endpoint and client ID (for cloud sync)
- `[NTP]` section: NTP server and timezone offset
- `[CloudSync]` section: Enable/disable cloud sync, sync interval

**Example**:
```ini
[WiFi]
SSID=HomeWiFi
Password=MySecurePassword123

[NTP]
Server=pool.ntp.org
TimezoneOffset=-18000  # EST (UTC-5)
```

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
