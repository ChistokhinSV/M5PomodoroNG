#ifndef SD_MANAGER_H
#define SD_MANAGER_H

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>

/**
 * @brief SD card manager for M5Stack Core2
 *
 * Provides abstraction for SD card operations with error handling
 * for card removal, corruption, and mount failures.
 *
 * File structure:
 * - /config/network.ini - Network configuration
 * - /config/certs/ - SSL certificates
 * - /config/lasttime.txt - Last known time (emergency fallback)
 * - /audio/ - WAV audio files
 */
class SDManager {
public:
    SDManager();
    ~SDManager();

    /**
     * @brief Initialize SD card and mount filesystem
     * @return true if SD card mounted successfully
     */
    bool begin();

    /**
     * @brief Check if SD card is currently mounted
     * @return true if mounted, false otherwise
     */
    bool isMounted() const;

    /**
     * @brief Check if file or directory exists
     * @param path Absolute path (e.g., "/config/network.ini")
     * @return true if exists
     */
    bool exists(const char* path);

    /**
     * @brief Read entire file as String
     * @param path Absolute path to file
     * @return File contents, empty string on error
     */
    String readFile(const char* path);

    /**
     * @brief Read file into buffer
     * @param path Absolute path to file
     * @param buffer Destination buffer
     * @param max_len Maximum bytes to read
     * @return Number of bytes read, 0 on error
     */
    size_t readFile(const char* path, uint8_t* buffer, size_t max_len);

    /**
     * @brief Write String to file (overwrites existing)
     * @param path Absolute path to file
     * @param data Data to write
     * @return true if successful
     */
    bool writeFile(const char* path, const String& data);

    /**
     * @brief Write buffer to file (overwrites existing)
     * @param path Absolute path to file
     * @param data Source buffer
     * @param len Number of bytes to write
     * @return true if successful
     */
    bool writeFile(const char* path, const uint8_t* data, size_t len);

    /**
     * @brief Append String to file (creates if doesn't exist)
     * @param path Absolute path to file
     * @param data Data to append
     * @return true if successful
     */
    bool appendFile(const char* path, const String& data);

    /**
     * @brief Delete file
     * @param path Absolute path to file
     * @return true if successful
     */
    bool deleteFile(const char* path);

    /**
     * @brief Create directory (creates parent dirs if needed)
     * @param path Absolute path to directory
     * @return true if successful or already exists
     */
    bool mkdir(const char* path);

    /**
     * @brief Remove directory (must be empty)
     * @param path Absolute path to directory
     * @return true if successful
     */
    bool rmdir(const char* path);

    /**
     * @brief List files in directory
     * @param path Directory path (e.g., "/audio")
     * @param callback Function called for each entry (name, isDir)
     */
    void listDir(const char* path, void (*callback)(const char* name, bool isDir));

    /**
     * @brief Get free space on SD card
     * @return Free space in MB, 0 if not mounted
     */
    uint32_t getFreeMB() const;

    /**
     * @brief Get total SD card capacity
     * @return Total space in MB, 0 if not mounted
     */
    uint32_t getTotalMB() const;

    /**
     * @brief Get SD card type
     * @return Card type string ("NONE", "MMC", "SD", "SDHC", "Unknown")
     */
    String getCardType() const;

    /**
     * @brief Unmount SD card
     */
    void end();

private:
    bool mounted_;

    /**
     * @brief Ensure parent directories exist
     * @param path File path
     * @return true if parent dirs exist or created
     */
    bool ensureParentDirs(const char* path);
};

#endif // SD_MANAGER_H
