#include "SDManager.h"
#include <M5Unified.h>

// M5Stack Core2 SD card pins (SPI mode)
#define SD_CS_PIN 4

SDManager::SDManager() : mounted_(false) {}

SDManager::~SDManager() {
    if (mounted_) {
        end();
    }
}

bool SDManager::begin() {
    // Attempt to mount SD card
    if (!SD.begin(SD_CS_PIN, SPI, 40000000)) {
        Serial.println("[SDManager] SD card mount failed");
        mounted_ = false;
        return false;
    }

    // Verify SD card is present and readable
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("[SDManager] No SD card detected");
        SD.end();
        mounted_ = false;
        return false;
    }

    mounted_ = true;

    // Print SD card info
    Serial.printf("[SDManager] SD card mounted: %s, %u MB total\n",
                  getCardType().c_str(), getTotalMB());

    return true;
}

bool SDManager::isMounted() const {
    return mounted_;
}

bool SDManager::exists(const char* path) {
    if (!mounted_) {
        return false;
    }

    return SD.exists(path);
}

String SDManager::readFile(const char* path) {
    if (!mounted_) {
        Serial.println("[SDManager] Cannot read: SD not mounted");
        return "";
    }

    if (!SD.exists(path)) {
        Serial.printf("[SDManager] File not found: %s\n", path);
        return "";
    }

    File file = SD.open(path, FILE_READ);
    if (!file) {
        Serial.printf("[SDManager] Failed to open file: %s\n", path);
        return "";
    }

    String content = "";
    while (file.available()) {
        content += (char)file.read();
    }

    file.close();
    Serial.printf("[SDManager] Read %d bytes from %s\n", content.length(), path);
    return content;
}

size_t SDManager::readFile(const char* path, uint8_t* buffer, size_t max_len) {
    if (!mounted_ || !buffer) {
        return 0;
    }

    if (!SD.exists(path)) {
        Serial.printf("[SDManager] File not found: %s\n", path);
        return 0;
    }

    File file = SD.open(path, FILE_READ);
    if (!file) {
        Serial.printf("[SDManager] Failed to open file: %s\n", path);
        return 0;
    }

    size_t fileSize = file.size();
    size_t toRead = (fileSize < max_len) ? fileSize : max_len;
    size_t bytesRead = file.read(buffer, toRead);

    file.close();
    Serial.printf("[SDManager] Read %d bytes from %s\n", bytesRead, path);
    return bytesRead;
}

bool SDManager::writeFile(const char* path, const String& data) {
    return writeFile(path, (const uint8_t*)data.c_str(), data.length());
}

bool SDManager::writeFile(const char* path, const uint8_t* data, size_t len) {
    if (!mounted_) {
        Serial.println("[SDManager] Cannot write: SD not mounted");
        return false;
    }

    if (!data || len == 0) {
        Serial.println("[SDManager] Invalid data or length");
        return false;
    }

    // Ensure parent directories exist
    if (!ensureParentDirs(path)) {
        Serial.printf("[SDManager] Failed to create parent dirs for %s\n", path);
        return false;
    }

    // Open file for writing (creates or overwrites)
    File file = SD.open(path, FILE_WRITE);
    if (!file) {
        Serial.printf("[SDManager] Failed to open file for writing: %s\n", path);
        return false;
    }

    // Write data
    size_t written = file.write(data, len);
    file.close();

    if (written != len) {
        Serial.printf("[SDManager] Write incomplete: %d/%d bytes\n", written, len);
        return false;
    }

    Serial.printf("[SDManager] Wrote %d bytes to %s\n", written, path);
    return true;
}

bool SDManager::appendFile(const char* path, const String& data) {
    if (!mounted_) {
        Serial.println("[SDManager] Cannot append: SD not mounted");
        return false;
    }

    // Ensure parent directories exist
    if (!ensureParentDirs(path)) {
        return false;
    }

    File file = SD.open(path, FILE_APPEND);
    if (!file) {
        Serial.printf("[SDManager] Failed to open file for append: %s\n", path);
        return false;
    }

    size_t written = file.print(data);
    file.close();

    Serial.printf("[SDManager] Appended %d bytes to %s\n", written, path);
    return (written == data.length());
}

bool SDManager::deleteFile(const char* path) {
    if (!mounted_) {
        return false;
    }

    if (!SD.exists(path)) {
        return false; // Already doesn't exist
    }

    bool result = SD.remove(path);
    if (result) {
        Serial.printf("[SDManager] Deleted file: %s\n", path);
    } else {
        Serial.printf("[SDManager] Failed to delete: %s\n", path);
    }

    return result;
}

bool SDManager::mkdir(const char* path) {
    if (!mounted_) {
        return false;
    }

    // Check if already exists
    if (SD.exists(path)) {
        return true; // Already exists
    }

    // SD.mkdir() creates all parent directories automatically
    bool result = SD.mkdir(path);
    if (result) {
        Serial.printf("[SDManager] Created directory: %s\n", path);
    } else {
        Serial.printf("[SDManager] Failed to create directory: %s\n", path);
    }

    return result;
}

bool SDManager::rmdir(const char* path) {
    if (!mounted_) {
        return false;
    }

    bool result = SD.rmdir(path);
    if (result) {
        Serial.printf("[SDManager] Removed directory: %s\n", path);
    } else {
        Serial.printf("[SDManager] Failed to remove directory: %s\n", path);
    }

    return result;
}

void SDManager::listDir(const char* path, void (*callback)(const char* name, bool isDir)) {
    if (!mounted_ || !callback) {
        return;
    }

    File root = SD.open(path);
    if (!root) {
        Serial.printf("[SDManager] Failed to open directory: %s\n", path);
        return;
    }

    if (!root.isDirectory()) {
        Serial.printf("[SDManager] Not a directory: %s\n", path);
        root.close();
        return;
    }

    File file = root.openNextFile();
    while (file) {
        callback(file.name(), file.isDirectory());
        file = root.openNextFile();
    }

    root.close();
}

uint32_t SDManager::getFreeMB() const {
    if (!mounted_) {
        return 0;
    }

    // Get free space in bytes, convert to MB
    uint64_t freeBytes = SD.totalBytes() - SD.usedBytes();
    return (uint32_t)(freeBytes / (1024 * 1024));
}

uint32_t SDManager::getTotalMB() const {
    if (!mounted_) {
        return 0;
    }

    uint64_t totalBytes = SD.totalBytes();
    return (uint32_t)(totalBytes / (1024 * 1024));
}

String SDManager::getCardType() const {
    if (!mounted_) {
        return "NONE";
    }

    uint8_t cardType = SD.cardType();
    switch (cardType) {
        case CARD_MMC:
            return "MMC";
        case CARD_SD:
            return "SD";
        case CARD_SDHC:
            return "SDHC";
        default:
            return "Unknown";
    }
}

void SDManager::end() {
    if (mounted_) {
        SD.end();
        mounted_ = false;
        Serial.println("[SDManager] SD card unmounted");
    }
}

bool SDManager::ensureParentDirs(const char* path) {
    if (!mounted_ || !path || path[0] != '/') {
        return false;
    }

    // Extract parent directory path
    String pathStr(path);
    int lastSlash = pathStr.lastIndexOf('/');

    if (lastSlash <= 0) {
        return true; // Root directory or no parent
    }

    String parentDir = pathStr.substring(0, lastSlash);

    // Check if parent exists
    if (SD.exists(parentDir.c_str())) {
        return true;
    }

    // Create parent directory (recursively creates all needed dirs)
    return mkdir(parentDir.c_str());
}
