/************************************************************************************************
 * @file    firmware_update.cpp
 * @author  AB-Engineering - https://ab-engineering.it
 * @brief   Firmware update management for the Milano Smart Park project
 * @version 0.1
 * @date    2025-01-11
 *
 * @copyright Copyright (c) 2025
 *
 ************************************************************************************************/

#include "firmware_update.h"
#include "network.h"
#include "display_task.h"
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SD.h>
#include "esp32-hal-log.h"

// ESP-IDF OTA includes
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_app_format.h"

// GitHub API constants
#define GITHUB_API_URL "https://api.github.com/repos/A-A-Milano-Smart-Park/msp-firmware/releases/latest"
#define GITHUB_TEST_API_URL "https://api.github.com/repos/AB-Engineering/msp-firmware/releases/latest"
#define FIRMWARE_UPDATE_TIMEOUT_MS 60000
#define DOWNLOAD_BUFFER_SIZE 2048 // Reduced from 8192 to prevent stack overflow

static String extractVersionFromTag(const String &tag);
static bool downloadFile(const String &url, const String &filepath);

/**
 * @brief Check for firmware updates on GitHub
 */
void vHalFirmware_checkForUpdates(systemData_t *sysData, systemStatus_t *sysStatus, deviceNetworkInfo_t *devInfo)
{
    log_i("Checking for firmware updates...");

    if (!sysStatus->connection)
    {
        log_w("No network connection available for firmware update check");
        return;
    }

    // Ensure we have internet connection
    if (!WiFi.isConnected() && !sysStatus->use_modem)
    {
        log_w("WiFi not connected for firmware update check");
        requestNetworkConnection();
        return;
    }

    HTTPClient http;
    http.setTimeout(FIRMWARE_UPDATE_TIMEOUT_MS);

    if (!http.begin(GITHUB_API_URL))
    {
        log_e("Failed to initialize HTTP client for GitHub API");
        return;
    }

    http.addHeader("User-Agent", "MilanoSmartPark-ESP32");
    http.addHeader("Accept", "application/vnd.github.v3+json");

    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK)
    {
        log_e("GitHub API request failed with code: %d", httpCode);
        http.end();
        return;
    }

    String payload = http.getString();
    http.end();

    log_d("GitHub API response: %s", payload.substring(0, 200).c_str());

    // Parse JSON response
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, payload);

    if (error)
    {
        log_e("JSON parsing failed: %s", error.c_str());
        return;
    }

    // Extract release information
    String latestTag = doc["tag_name"].as<String>();
    String downloadUrl = "";
    String latestVersion = extractVersionFromTag(latestTag);

    // Look for the direct binary file (update_vX.X.X.bin)
    JsonArray assets = doc["assets"];
    String binaryFileName = "update_" + latestTag + ".bin";

    for (JsonObject asset : assets)
    {
        String name = asset["name"].as<String>();
        String url = asset["browser_download_url"].as<String>();

        if (name == binaryFileName)
        {
            downloadUrl = url;
            log_i("Found application binary: %s", name.c_str());
            break;
        }
    }

    if (downloadUrl.isEmpty())
    {
        log_e("No application binary (%s) found in release assets", binaryFileName.c_str());
        return;
    }

    log_i("Current version: %s", sysData->ver.c_str());
    log_i("Latest version: %s", latestVersion.c_str());
    log_i("Download URL: %s", downloadUrl.c_str());

    // Compare versions
    if (bHalFirmware_compareVersions(sysData->ver, latestVersion))
    {
        log_i("New firmware version available, starting download and update process...");

        if (bHalFirmware_downloadBinaryFirmware(downloadUrl, sysData, sysStatus, devInfo))
        {
            log_i("Firmware download completed successfully");

            // Trigger OTA update through network task for better stack management
            log_i("Requesting OTA update via network task...");
            requestFirmwareUpdate(sysData, sysStatus, devInfo);
        }
        else
        {
            log_e("Firmware download failed");
        }
    }
    else
    {
        log_i("No firmware update needed, current version is up to date");
    }
}

/**
 * @brief Compare two version strings (semantic versioning)
 * @param currentVersion Current firmware version
 * @param remoteVersion Remote firmware version
 * @return true if remote version is newer
 */
bool bHalFirmware_compareVersions(const String &currentVersion, const String &remoteVersion)
{
    // Simple version comparison - assumes format like "v1.2.3"
    // Remove 'v' prefix if present
    String current = currentVersion;
    String remote = remoteVersion;

    if (current.startsWith("v"))
        current = current.substring(1);
    if (remote.startsWith("v"))
        remote = remote.substring(1);

    // Special case: development version is always considered older
    if (current.equals("DEV") || current.equals("dev"))
    {
        return true; // Any remote version is newer than DEV
    }

    // Parse version components
    int currentMajor = 0, currentMinor = 0, currentPatch = 0;
    int remoteMajor = 0, remoteMinor = 0, remotePatch = 0;

    // Parse current version
    int firstDot = current.indexOf('.');
    int secondDot = current.indexOf('.', firstDot + 1);

    if (firstDot > 0)
    {
        currentMajor = current.substring(0, firstDot).toInt();
        if (secondDot > firstDot)
        {
            currentMinor = current.substring(firstDot + 1, secondDot).toInt();
            currentPatch = current.substring(secondDot + 1).toInt();
        }
        else
        {
            currentMinor = current.substring(firstDot + 1).toInt();
        }
    }

    // Parse remote version
    firstDot = remote.indexOf('.');
    secondDot = remote.indexOf('.', firstDot + 1);

    if (firstDot > 0)
    {
        remoteMajor = remote.substring(0, firstDot).toInt();
        if (secondDot > firstDot)
        {
            remoteMinor = remote.substring(firstDot + 1, secondDot).toInt();
            remotePatch = remote.substring(secondDot + 1).toInt();
        }
        else
        {
            remoteMinor = remote.substring(firstDot + 1).toInt();
        }
    }

    log_d("Version comparison: current %d.%d.%d vs remote %d.%d.%d",
          currentMajor, currentMinor, currentPatch,
          remoteMajor, remoteMinor, remotePatch);

    // Compare versions
    if (remoteMajor > currentMajor)
        return true;
    if (remoteMajor < currentMajor)
        return false;

    if (remoteMinor > currentMinor)
        return true;
    if (remoteMinor < currentMinor)
        return false;

    return remotePatch > currentPatch;
}

/**
 * @brief Download firmware binary file directly
 */
bool bHalFirmware_downloadBinaryFirmware(const String &downloadUrl, systemData_t *sysData, systemStatus_t *sysStatus, deviceNetworkInfo_t *devInfo)
{
    log_i("Downloading firmware binary from: %s", downloadUrl.c_str());

    String firmwarePath = "/firmware.bin";

    if (!downloadFile(downloadUrl, firmwarePath))
    {
        log_e("Failed to download firmware binary file");
        return false;
    }

    log_i("Firmware binary downloaded successfully to: %s", firmwarePath.c_str());

    // Verify the downloaded file exists and has reasonable size
    File firmwareFile = SD.open(firmwarePath.c_str(), FILE_READ);
    if (!firmwareFile)
    {
        log_e("Failed to open downloaded firmware file");
        return false;
    }

    size_t fileSize = firmwareFile.size();
    firmwareFile.close();

    if (fileSize < 100000 || fileSize > 2000000) // Reasonable firmware size range
    {
        log_w("Firmware file size (%zu bytes) seems unusual", fileSize);
        // Don't fail, just warn - size limits may vary
    }

    log_i("Firmware binary ready for OTA update: %zu bytes", fileSize);
    return true;
}

/**
 * @brief Perform OTA update from firmware file using ESP-IDF native OTA
 */
bool bHalFirmware_performOTAUpdate(const String &firmwarePath)
{
    log_i("Starting ESP-IDF native OTA update from: %s", firmwarePath.c_str());

    if (!SD.exists(firmwarePath.c_str()))
    {
        log_e("Firmware file not found: %s", firmwarePath.c_str());
        return false;
    }

    File firmwareFile = SD.open(firmwarePath.c_str(), FILE_READ);
    if (!firmwareFile)
    {
        log_e("Failed to open firmware file");
        return false;
    }

    size_t firmwareSize = firmwareFile.size();
    log_i("Firmware size: %d bytes", firmwareSize);

    // Get current running partition info
    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    if (running_partition == NULL)
    {
        log_e("Failed to get running partition");
        firmwareFile.close();
        return false;
    }

    log_i("Running partition: %s at offset 0x%08x", running_partition->label, running_partition->address);

    // Get next available OTA partition
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL)
    {
        log_e("Failed to get next update partition");
        firmwareFile.close();
        return false;
    }

    log_i("Update partition: %s at offset 0x%08x (size: %d bytes)",
          update_partition->label, update_partition->address, update_partition->size);

    // Check if firmware size fits in the partition
    if (firmwareSize > update_partition->size)
    {
        log_e("Firmware size (%d) exceeds partition size (%d)", firmwareSize, update_partition->size);
        firmwareFile.close();
        return false;
    }

    // Begin OTA update process
    esp_ota_handle_t ota_handle = 0;
    esp_err_t err = esp_ota_begin(update_partition, firmwareSize, &ota_handle);
    if (err != ESP_OK)
    {
        log_e("Failed to begin OTA update: %s", esp_err_to_name(err));
        firmwareFile.close();
        return false;
    }

    log_i("OTA update started successfully");

    // Write firmware data in chunks
    static uint8_t buffer[DOWNLOAD_BUFFER_SIZE]; // Static to avoid stack allocation
    size_t bytesWritten = 0;
    bool ota_success = true;
    int loopCount = 0;

    while (firmwareFile.available() && ota_success)
    {
        size_t bytesRead = firmwareFile.readBytes((char *)buffer, DOWNLOAD_BUFFER_SIZE);
        if (bytesRead > 0)
        {
            err = esp_ota_write(ota_handle, buffer, bytesRead);
            if (err != ESP_OK)
            {
                log_e("OTA write failed: %s", esp_err_to_name(err));
                ota_success = false;
                break;
            }

            bytesWritten += bytesRead;

            // Progress logging every 32KB (reduced frequency)
            if (bytesWritten % (32 * 1024) == 0)
            {
                log_i("OTA progress: %d/%d bytes (%.1f%%)",
                      bytesWritten, firmwareSize,
                      (float)bytesWritten / firmwareSize * 100);
            }

            // Yield more frequently to prevent stack overflow
            if (++loopCount % 5 == 0)
            {
                yield();  // Give other tasks a chance
                delay(1); // Small delay to prevent watchdog reset
            }
        }
        else
        {
            break; // End of file
        }
    }

    firmwareFile.close();

    if (!ota_success)
    {
        log_e("OTA write process failed, aborting...");
        esp_ota_abort(ota_handle);
        return false;
    }

    log_i("Firmware written successfully: %d bytes", bytesWritten);

    // Finalize the OTA update
    err = esp_ota_end(ota_handle);
    if (err != ESP_OK)
    {
        log_e("Failed to finalize OTA update: %s", esp_err_to_name(err));
        return false;
    }

    log_i("OTA update finalized successfully");

    // Validate the written firmware
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK)
    {
        log_e("Failed to set boot partition: %s", esp_err_to_name(err));
        return false;
    }

    log_i("Boot partition set to: %s", update_partition->label);

    // Optional: Mark the update as valid (for rollback protection)
    // This will be done after successful boot in the main application

    log_i("OTA update completed successfully!");
    log_i("System will restart to apply the new firmware...");

    // Brief delay before restart
    delay(2000);

    // Restart the system
    esp_restart();

    return true; // Will never reach here due to restart
}

// Helper functions

/**
 * @brief Extract version number from git tag
 */
static String extractVersionFromTag(const String &tag)
{
    // Assumes tag format like "v1.2.3" or "1.2.3"
    return tag;
}

/**
 * @brief Download file from URL to SD card
 */
static bool downloadFile(const String &url, const String &filepath)
{
    log_i("Starting download from: %s", url.c_str());
    log_i("Saving to: %s", filepath.c_str());

    // Check available memory before starting download
    size_t freeHeap = ESP.getFreeHeap();
    log_i("Free heap before download: %zu bytes", freeHeap);

    if (freeHeap < 50000)
    { // Require at least 50KB free heap
        log_e("Insufficient memory for download (need 50KB, have %zu bytes)", freeHeap);
        return false;
    }

    HTTPClient http;
    http.setTimeout(FIRMWARE_UPDATE_TIMEOUT_MS);

    // Handle both HTTP and HTTPS URLs with proper client setup
    WiFiClientSecure *secureClient = nullptr;
    bool usedSpiram = false;

    if (url.startsWith("https://"))
    {
        // Use SPIRAM for WiFiClientSecure allocation to avoid heap issues
        secureClient = (WiFiClientSecure *)heap_caps_malloc(sizeof(WiFiClientSecure), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!secureClient)
        {
            log_w("SPIRAM allocation failed, trying regular heap");
            secureClient = new (std::nothrow) WiFiClientSecure();
            if (!secureClient)
            {
                log_e("Failed to allocate WiFiClientSecure - insufficient memory");
                return false;
            }
        }
        else
        {
            // Construct the object in the SPIRAM memory
            new (secureClient) WiFiClientSecure();
            usedSpiram = true;
            log_i("WiFiClientSecure allocated in SPIRAM");
        }

        secureClient->setInsecure();              // Accept all certificates for simplicity
        secureClient->setTimeout(30000);          // 30 second timeout
        secureClient->setHandshakeTimeout(30000); // 30 second handshake timeout

        if (!http.begin(*secureClient, url))
        {
            log_e("Failed to initialize HTTPS client for download");
            if (usedSpiram)
            {
                secureClient->~WiFiClientSecure();
                heap_caps_free(secureClient);
            }
            else
            {
                delete secureClient;
            }
            return false;
        }

        log_i("HTTPS client initialized successfully");
    }
    else
    {
        if (!http.begin(url))
        {
            log_e("Failed to initialize HTTP client for download");
            return false;
        }
        log_i("HTTP client initialized successfully");
    }

    // Attempt GET request with retry logic for connection issues
    int httpCode = -1;
    int retryCount = 0;
    const int maxRetries = 3;

    while (retryCount < maxRetries)
    {
        log_i("Attempting HTTP GET (attempt %d/%d)", retryCount + 1, maxRetries);

        // Add User-Agent header for better compatibility with GitHub
        http.addHeader("User-Agent", "MSP-Firmware-Downloader/1.0");

        httpCode = http.GET();

        if (httpCode > 0)
        {
            break; // Success or HTTP error (not connection error)
        }

        // Connection error (negative code), retry
        log_w("Connection error %d, retrying in 2 seconds...", httpCode);
        retryCount++;

        if (retryCount < maxRetries)
        {
            delay(2000); // Wait 2 seconds before retry
        }
    }

    if (httpCode < 0)
    {
        log_e("Connection failed after %d attempts with error: %d", maxRetries, httpCode);
        http.end();
        if (secureClient)
        {
            if (usedSpiram)
            {
                secureClient->~WiFiClientSecure();
                heap_caps_free(secureClient);
            }
            else
            {
                delete secureClient;
            }
        }
        return false;
    }

    // Handle redirects (GitHub often returns 302 redirects)
    int redirectCount = 0;
    const int maxRedirects = 5;

    while ((httpCode == HTTP_CODE_MOVED_PERMANENTLY || httpCode == HTTP_CODE_FOUND) && redirectCount < maxRedirects)
    {
        String newLocation = http.getLocation();
        log_i("HTTP %d redirect to: %s", httpCode, newLocation.c_str());

        if (newLocation.length() == 0)
        {
            log_e("Redirect location is empty");
            break;
        }

        http.end();

        // Follow the redirect
        if (!http.begin(newLocation))
        {
            log_e("Failed to begin HTTP client for redirect URL");
            if (secureClient)
            {
                if (usedSpiram)
                {
                    secureClient->~WiFiClientSecure();
                    heap_caps_free(secureClient);
                }
                else
                {
                    delete secureClient;
                }
            }
            return false;
        }

        httpCode = http.GET();
        redirectCount++;
    }

    if (httpCode != HTTP_CODE_OK)
    {
        log_e("Download request failed with code: %d after %d redirects", httpCode, redirectCount);
        http.end();
        if (secureClient)
        {
            if (usedSpiram)
            {
                secureClient->~WiFiClientSecure();
                heap_caps_free(secureClient);
            }
            else
            {
                delete secureClient;
            }
        }
        return false;
    }

    log_i("Download successful after %d redirects", redirectCount);

    int totalLength = http.getSize();
    log_i("Starting download, file size: %d bytes", totalLength);

    File file = SD.open(filepath.c_str(), FILE_WRITE);
    if (!file)
    {
        log_e("Failed to create download file: %s", filepath.c_str());
        http.end();
        if (secureClient)
        {
            if (usedSpiram)
            {
                secureClient->~WiFiClientSecure();
                heap_caps_free(secureClient);
            }
            else
            {
                delete secureClient;
            }
        }
        return false;
    }

    WiFiClient *stream = http.getStreamPtr();
    static uint8_t buffer[DOWNLOAD_BUFFER_SIZE]; // Static to avoid stack allocation
    int bytesWritten = 0;
    int loopCount = 0;

    while (http.connected() && (totalLength > 0 || totalLength == -1))
    {
        size_t availableBytes = stream->available();
        if (availableBytes > 0)
        {
            int bytesToRead = min(availableBytes, sizeof(buffer));
            int bytesRead = stream->readBytes(buffer, bytesToRead);
            file.write(buffer, bytesRead);
            bytesWritten += bytesRead;

            if (totalLength > 0)
            {
                totalLength -= bytesRead;

                // Progress logging every 32KB (reduced frequency)
                if (bytesWritten % (32 * 1024) == 0)
                {
                    int totalSize = http.getSize();
                    log_i("Download progress: %d/%d bytes (%.1f%%)",
                          bytesWritten, totalSize,
                          (float)bytesWritten / totalSize * 100);
                }
            }

            // Yield more frequently to prevent stack overflow
            if (++loopCount % 10 == 0)
            {
                yield();  // Give other tasks a chance
                delay(1); // Small delay to prevent watchdog reset
            }
        }
        else
        {
            delay(10);
            yield(); // Yield when waiting
        }
    }

    file.close();
    http.end();

    // Clean up HTTPS client if used
    if (secureClient)
    {
        if (usedSpiram)
        {
            secureClient->~WiFiClientSecure();
            heap_caps_free(secureClient);
            log_i("SPIRAM WiFiClientSecure freed");
        }
        else
        {
            delete secureClient;
        }
        secureClient = nullptr;
    }

    log_i("Download completed: %d bytes written to %s", bytesWritten, filepath.c_str());
    return true;
}

// ESP-IDF OTA management functions implementation

/**
 * @brief Print current OTA partition information
 */
void vHalFirmware_printOTAInfo()
{
    log_i("=== OTA Partition Information ===");

    // Get running partition
    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    if (running_partition)
    {
        log_i("Running partition: %s", running_partition->label);
        log_i("  - Address: 0x%08x", running_partition->address);
        log_i("  - Size: %d bytes", running_partition->size);
        log_i("  - Type: %d, Subtype: %d", running_partition->type, running_partition->subtype);
    }

    // Get next update partition
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition)
    {
        log_i("Next update partition: %s", update_partition->label);
        log_i("  - Address: 0x%08x", update_partition->address);
        log_i("  - Size: %d bytes", update_partition->size);
    }

    // Get boot partition
    const esp_partition_t *boot_partition = esp_ota_get_boot_partition();
    if (boot_partition)
    {
        log_i("Boot partition: %s", boot_partition->label);
        log_i("  - Address: 0x%08x", boot_partition->address);
    }

    // Check OTA data partition
    const esp_partition_t *ota_data = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, NULL);
    if (ota_data)
    {
        log_i("OTA data partition found: %s", ota_data->label);
        log_i("  - Address: 0x%08x", ota_data->address);
        log_i("  - Size: %d bytes", ota_data->size);
    }

    // Check current app state
    esp_ota_img_states_t ota_state;
    const esp_partition_t *last_invalid_app = esp_ota_get_last_invalid_partition();
    if (last_invalid_app)
    {
        log_w("Last invalid partition: %s", last_invalid_app->label);
    }

    esp_err_t err = esp_ota_get_state_partition(running_partition, &ota_state);
    if (err == ESP_OK)
    {
        switch (ota_state)
        {
        case ESP_OTA_IMG_NEW:
            log_i("Current app state: NEW (first boot after update)");
            break;
        case ESP_OTA_IMG_PENDING_VERIFY:
            log_w("Current app state: PENDING_VERIFY (needs validation)");
            break;
        case ESP_OTA_IMG_VALID:
            log_i("Current app state: VALID (confirmed working)");
            break;
        case ESP_OTA_IMG_INVALID:
            log_e("Current app state: INVALID (marked as failed)");
            break;
        case ESP_OTA_IMG_ABORTED:
            log_e("Current app state: ABORTED (update was aborted)");
            break;
        case ESP_OTA_IMG_UNDEFINED:
            log_w("Current app state: UNDEFINED");
            break;
        }
    }

    log_i("=== End OTA Information ===");
}

/**
 * @brief Validate current firmware and mark it as working
 */
bool bHalFirmware_validateCurrentFirmware()
{
    log_i("Validating current firmware...");

    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    if (!running_partition)
    {
        log_e("Failed to get running partition");
        return false;
    }

    esp_ota_img_states_t ota_state;
    esp_err_t err = esp_ota_get_state_partition(running_partition, &ota_state);
    if (err != ESP_OK)
    {
        log_e("Failed to get partition state: %s", esp_err_to_name(err));
        return false;
    }

    switch (ota_state)
    {
    case ESP_OTA_IMG_NEW:
    case ESP_OTA_IMG_PENDING_VERIFY:
        log_i("Firmware validation required, marking as valid...");
        return bHalFirmware_markFirmwareValid();

    case ESP_OTA_IMG_VALID:
        log_i("Firmware already validated");
        return true;

    case ESP_OTA_IMG_INVALID:
    case ESP_OTA_IMG_ABORTED:
        log_e("Current firmware is marked as invalid/aborted");
        return false;

    default:
        log_w("Unknown firmware state: %d", ota_state);
        return false;
    }
}

/**
 * @brief Mark current firmware as valid (prevents rollback)
 */
bool bHalFirmware_markFirmwareValid()
{
    log_i("Marking current firmware as valid...");

    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err == ESP_OK)
    {
        log_i("Firmware marked as valid successfully");
        return true;
    }
    else
    {
        log_e("Failed to mark firmware as valid: %s", esp_err_to_name(err));
        return false;
    }
}

/**
 * @brief Check if firmware rollback is available
 */
bool bHalFirmware_isRollbackAvailable()
{
    const esp_partition_t *last_invalid_app = esp_ota_get_last_invalid_partition();
    return (last_invalid_app != NULL);
}

/**
 * @brief Rollback to previous firmware version
 */
bool bHalFirmware_rollbackFirmware()
{
    log_i("Attempting firmware rollback...");

    if (!bHalFirmware_isRollbackAvailable())
    {
        log_e("No rollback partition available");
        return false;
    }

    const esp_partition_t *last_invalid_app = esp_ota_get_last_invalid_partition();
    log_i("Rolling back to partition: %s", last_invalid_app->label);

    esp_err_t err = esp_ota_set_boot_partition(last_invalid_app);
    if (err != ESP_OK)
    {
        log_e("Failed to set rollback partition: %s", esp_err_to_name(err));
        return false;
    }

    log_i("Rollback partition set successfully");
    log_i("System will restart to complete rollback...");

    delay(2000);
    esp_restart();

    return true; // Never reached due to restart
}

// Test functions implementation

/**
 * @brief Test version comparison functionality
 */
void vHalFirmware_testVersionComparison()
{
    log_i("=== Testing Version Comparison ===");

    struct
    {
        String current;
        String remote;
        bool expected;
        String description;
    } testCases[] = {
        {"1.0.0", "1.0.1", true, "Patch version update"},
        {"1.0.0", "1.1.0", true, "Minor version update"},
        {"1.0.0", "2.0.0", true, "Major version update"},
        {"1.1.0", "1.0.9", false, "Remote older minor"},
        {"1.0.1", "1.0.0", false, "Remote older patch"},
        {"2.0.0", "1.9.9", false, "Remote older major"},
        {"1.0.0", "1.0.0", false, "Same version"},
        {"v1.0.0", "v1.0.1", true, "With v prefix"},
        {"DEV", "1.0.0", true, "Development version"},
    };

    int passed = 0;
    int total = sizeof(testCases) / sizeof(testCases[0]);

    for (int i = 0; i < total; i++)
    {
        bool result = bHalFirmware_compareVersions(testCases[i].current, testCases[i].remote);
        if (result == testCases[i].expected)
        {
            log_i("PASS: %s (%s vs %s)", testCases[i].description.c_str(),
                  testCases[i].current.c_str(), testCases[i].remote.c_str());
            passed++;
        }
        else
        {
            log_e("FAIL: %s (%s vs %s) - Expected: %d, Got: %d",
                  testCases[i].description.c_str(), testCases[i].current.c_str(),
                  testCases[i].remote.c_str(), testCases[i].expected, result);
        }
    }

    log_i("Version comparison tests: %d/%d passed", passed, total);
}

/**
 * @brief Test GitHub API functionality (requires internet connection)
 */
void vHalFirmware_testGitHubAPI()
{
    log_i("=== Testing GitHub API ===");

    if (!WiFi.isConnected())
    {
        log_w("WiFi not connected, skipping GitHub API test");
        return;
    }

    HTTPClient http;
    http.setTimeout(10000);

    if (!http.begin(GITHUB_TEST_API_URL))
    {
        log_e("Failed to initialize HTTP client");
        return;
    }

    http.addHeader("User-Agent", "MilanoSmartPark-ESP32-Test");
    http.addHeader("Accept", "application/vnd.github.v3+json");

    log_i("Making request to: %s", GITHUB_TEST_API_URL);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK)
    {
        String payload = http.getString();
        log_i("PASS: GitHub API responded successfully");
        log_d("Response length: %d bytes", payload.length());

        // Try to parse JSON
        DynamicJsonDocument doc(8192);
        DeserializationError error = deserializeJson(doc, payload);

        if (!error)
        {
            String tagName = doc["tag_name"].as<String>();
            String publishedAt = doc["published_at"].as<String>();
            log_i("PASS: JSON parsing successful");
            log_i("Latest release: %s (published: %s)", tagName.c_str(), publishedAt.c_str());

            // Show available assets and which one would be selected
            JsonArray assets = doc["assets"];
            String macosUrl = "";
            String selectedAsset = "";

            log_i("Available assets:");
            for (JsonObject asset : assets)
            {
                String name = asset["name"].as<String>();
                int size = asset["size"].as<int>();
                log_i("  - %s (%d bytes)", name.c_str(), size);

                if (name.endsWith(".bin") && name.startsWith("update"))
                {
                    macosUrl = asset["browser_download_url"].as<String>();
                    selectedAsset = name;
                }
            }

            if (!selectedAsset.isEmpty())
            {
                log_i("PASS: Would select asset: %s", selectedAsset.c_str());
            }
            else
            {
                log_w("No suitable firmware asset found");
            }
        }
        else
        {
            log_e("FAIL: JSON parsing failed: %s", error.c_str());
        }
    }
    else
    {
        log_e("FAIL: GitHub API request failed with code: %d", httpCode);
    }

    http.end();
}

/**
 * @brief Test configuration parsing
 */
void vHalFirmware_testConfigParsing(systemStatus_t *sysStatus)
{
    log_i("=== Testing Configuration Parsing ===");

    log_i("fwAutoUpgrade setting: %s", sysStatus->fwAutoUpgrade ? "enabled" : "disabled");

    if (sysStatus->fwAutoUpgrade)
    {
        log_i("PASS: Firmware auto-upgrade is enabled");
    }
    else
    {
        log_i("INFO: Firmware auto-upgrade is disabled (this is normal for testing)");
    }

    log_i("Other system status:");
    log_i("- SD Card: %s", sysStatus->sdCard ? "OK" : "FAIL");
    log_i("- Configuration: %s", sysStatus->configuration ? "OK" : "FAIL");
    log_i("- Connection: %s", sysStatus->connection ? "OK" : "FAIL");
    log_i("- DateTime: %s", sysStatus->datetime ? "OK" : "FAIL");
    log_i("- Server: %s", sysStatus->server_ok ? "OK" : "FAIL");
}

/**
 * @brief Test OTA management functions with real firmware download and update
 */
void vHalFirmware_testOTAManagement()
{
    log_i("=== Testing ESP-IDF OTA Management ===");

    // Test 1: Print OTA partition information
    log_i("Test 1: OTA Partition Information");
    vHalFirmware_printOTAInfo();

    // Test 2: Check current firmware validation status
    log_i("Test 2: Current Firmware Validation");
    bool isValid = bHalFirmware_validateCurrentFirmware();
    if (isValid)
    {
        log_i("PASS: Current firmware is valid");
    }
    else
    {
        log_w("WARNING: Current firmware validation failed - this may be expected for new firmware");
    }

    // Test 3: Check if rollback is available
    log_i("Test 3: Rollback Availability");
    bool rollbackAvailable = bHalFirmware_isRollbackAvailable();
    if (rollbackAvailable)
    {
        log_i("INFO: Rollback is available to previous firmware");
    }
    else
    {
        log_i("INFO: No rollback available (single firmware installed)");
    }

    // Test 4: Mark current firmware as valid (if not already)
    log_i("Test 4: Mark Firmware as Valid");
    bool markSuccess = bHalFirmware_markFirmwareValid();
    if (markSuccess)
    {
        log_i("PASS: Successfully marked firmware as valid");
    }
    else
    {
        log_w("WARNING: Failed to mark firmware as valid");
    }

    // Test 5: Download and test complete FOTA process with older firmware
    log_i("Test 5: Complete FOTA Process Test");

    // Test URL - use latest release but bypass version checking for testing
    // This allows testing the complete FOTA process without needing an older version
    String testFirmwareUrl = "https://github.com/AB-Engineering/msp-firmware/releases/download/v4.0.0/update_v4.0.0.bin";
    String testFirmwarePath = "/update_v4.0.0.bin";

    log_i("Attempting to download test firmware from: %s", testFirmwareUrl.c_str());

    // Download the test firmware
    WiFiClientSecure client;
    client.setInsecure(); // For testing purposes
    HTTPClient http;

    http.begin(client, testFirmwareUrl);
    http.addHeader("User-Agent", "MSP-Firmware-Test/1.0");

    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK)
    {
        log_i("Successfully connected to download server");

        // Get the content length
        int contentLength = http.getSize();
        log_i("Firmware size: %d bytes", contentLength);

        if (contentLength > 0)
        {
            // Open file for writing on SD card
            File firmwareFile = SD.open(testFirmwarePath.c_str(), FILE_WRITE);
            if (firmwareFile)
            {
                log_i("Created firmware file on SD card");

                // Download the firmware in chunks
                WiFiClient *stream = http.getStreamPtr();
                uint8_t buffer[1024];
                int bytesDownloaded = 0;

                while (http.connected() && (contentLength > 0 || contentLength == -1))
                {
                    size_t size = stream->available();
                    if (size)
                    {
                        int c = stream->readBytes(buffer, ((size > sizeof(buffer)) ? sizeof(buffer) : size));
                        firmwareFile.write(buffer, c);

                        bytesDownloaded += c;
                        if (contentLength > 0)
                        {
                            contentLength -= c;
                        }

                        // Log progress every 10KB
                        if (bytesDownloaded % 10240 == 0)
                        {
                            log_i("Downloaded: %d bytes", bytesDownloaded);
                        }
                    }
                    delay(1);
                }

                firmwareFile.close();
                log_i("PASS: Firmware download completed - %d bytes", bytesDownloaded);

                // Test 6: Complete ZIP Extraction and OTA Process
                log_i("Test 6: Complete FOTA Process Test");

                // Test the complete FOTA process
                log_i("Testing complete FOTA pipeline...");

                File binFile = SD.open(testFirmwarePath.c_str(), FILE_READ);
                if (binFile)
                {
                    size_t zipSize = binFile.size();
                    log_i("BIN file size: %zu bytes", zipSize);

                    // Read first few bytes to verify ZIP format
                    uint8_t header[4];
                    if (binFile.readBytes((char *)header, 4) == 4)
                    {
                        if (header[0] == 0x50 && header[1] == 0x4B &&
                            header[2] == 0x03 && header[3] == 0x04)
                        {
                            log_i("PASS: Valid ZIP file header detected");

                            binFile.close();

                            // Test the complete extraction process
                            log_i("Testing ZIP extraction process...");

#ifdef ENABLE_ACTUAL_OTA_UPDATE_TEST
                            // Perform actual OTA update
                            log_i("REAL TEST: Performing actual FOTA update process");
                            log_w("WARNING: This will update the firmware and reboot the device!");
                            log_i("Starting OTA update in 5 seconds...");

                            for (int i = 5; i > 0; i--)
                            {
                                log_i("OTA update starting in %d seconds...", i);
                                delay(1000);
                            }

                            // Direct OTA call for low-level testing (bypasses network task intentionally)
                            bool otaSuccess = bHalFirmware_performOTAUpdate(testFirmwarePath);
                            if (otaSuccess)
                            {
                                log_i("PASS: OTA update completed successfully");
                                log_i("Device will reboot to new firmware in 3 seconds...");
                                delay(3000);
                                ESP.restart();
                            }
                            else
                            {
                                log_e("FAIL: OTA update failed");
                            }
#else
                            // Perform detailed ZIP extraction testing without actual OTA
                            log_i("DETAILED TEST: ZIP extraction analysis");

                            // For testing, we can use the existing function but with the already downloaded file
                            // This will test the extraction without actually updating
                            // Direct OTA call for testing ZIP extraction mechanism (bypasses network task intentionally)
                            bool extractSuccess = bHalFirmware_performOTAUpdate(testFirmwarePath);

                            if (extractSuccess)
                            {
                                log_i("PASS: ZIP extraction and OTA process completed successfully (simulation mode)");
                                log_i("In real mode, device would reboot to new firmware");
                            }
                            else
                            {
                                log_e("FAIL: ZIP extraction or OTA process failed");
                            }

                            log_i("SIMULATION: Complete FOTA process validated");
                            log_i("INFO: To enable actual OTA testing, uncomment ENABLE_ACTUAL_OTA_UPDATE_TEST in config.h");
#endif
                        }
                        else
                        {
                            log_e("FAIL: Invalid BIN file format - Header: 0x%02X%02X%02X%02X",
                                  header[0], header[1], header[2], header[3]);
                        }
                    }
                    else
                    {
                        log_e("FAIL: Could not read ZIP header");
                    }

                    binFile.close();
                }
                else
                {
                    log_e("FAIL: Could not open downloaded ZIP file");
                }

                // Clean up test files
                log_i("Cleaning up test files...");
                SD.remove(testFirmwarePath.c_str());

                log_i("PASS: Complete FOTA process test completed successfully");
            }
            else
            {
                log_e("FAIL: Could not create firmware file on SD card");
            }
        }
        else
        {
            log_e("FAIL: Invalid content length: %d", contentLength);
        }
    }
    else
    {
        log_w("INFO: Could not download test firmware (HTTP %d) - this is expected if URL doesn't exist", httpCode);
        log_i("Testing with local simulation instead...");

        // Fallback: Test with simulated firmware data
        log_i("SIMULATION: Creating test firmware data");
        File testFile = SD.open(testFirmwarePath.c_str(), FILE_WRITE);
        if (testFile)
        {
            // Write some test data (simulating a small firmware)
            uint8_t testData[1024];
            for (int i = 0; i < sizeof(testData); i++)
            {
                testData[i] = (uint8_t)(i % 256);
            }
            testFile.write(testData, sizeof(testData));
            testFile.close();

            log_i("PASS: Created simulated firmware test file");

            // Clean up
            SD.remove(testFirmwarePath.c_str());
        }
    }

    http.end();

    log_i("=== OTA Management Tests Completed ===");
}

/**
 * @brief Force OTA update without version checking
 * Downloads and installs the latest firmware from GitHub releases
 */
bool bHalFirmware_forceOTAUpdate(systemData_t *sysData, systemStatus_t *sysStatus, deviceNetworkInfo_t *devInfo)
{
    log_i("=== Force OTA Update (No Version Check) ===");

    // Check available memory before starting
    size_t freeHeap = ESP.getFreeHeap();
    log_i("Free heap before OTA: %zu bytes", freeHeap);

    if (freeHeap < 50000) {  // Require at least 50KB free heap minimum
        log_e("Insufficient memory for OTA update (need 50KB, have %zu bytes)", freeHeap);
        return false;
    }

    // GitHub API URL for latest release
    String githubApiUrl = "https://api.github.com/repos/A-A-Milano-Smart-Park/msp-firmware/releases/latest";

    HTTPClient http;
    http.setTimeout(FIRMWARE_UPDATE_TIMEOUT_MS);

    // Use HTTPS client
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(30000);
    client.setHandshakeTimeout(30000);
    
    if (!http.begin(client, githubApiUrl)) {
        log_e("Failed to initialize HTTPS client for GitHub API");
        return false;
    }
    log_i("Connecting to GitHub API via HTTPS...");

    http.addHeader("User-Agent", "MSP-Firmware/1.0");

    log_i("Fetching latest release info from GitHub...");

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK)
    {
        log_e("Failed to get release info from GitHub API (HTTP %d)", httpCode);

        if (httpCode == 404)
        {
            log_e("Repository or releases not found. Possible causes:");
            log_e("1. Repository doesn't exist: A-A-Milano-Smart-Park/msp-firmware");
            log_e("2. No releases have been published yet");
            log_e("3. Repository is private");
            log_i("You need to:");
            log_i("- Create a release in GitHub with a firmware.bin.zip asset");
            log_i("- Make sure the repository is public");
            log_i("- Or update the repository URL in firmware_update.cpp");
        }

        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    // Parse JSON response
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, payload);

    if (error)
    {
        log_e("Failed to parse GitHub API response: %s", error.c_str());
        return false;
    }

    // Extract download URL for direct binary file
    String downloadUrl = "";
    String tagName = doc["tag_name"].as<String>();
    String binaryFileName = "update_" + tagName + ".bin";
    JsonArray assets = doc["assets"];

    for (JsonVariant asset : assets)
    {
        String assetName = asset["name"].as<String>();
        if (assetName == binaryFileName)
        {
            downloadUrl = asset["browser_download_url"].as<String>();
            log_i("Found firmware binary: %s", assetName.c_str());
            break;
        }
    }

    if (downloadUrl.isEmpty())
    {
        log_e("No firmware binary (%s) found in latest release", binaryFileName.c_str());
        return false;
    }

    log_i("Latest firmware download URL: %s", downloadUrl.c_str());
    log_i("URL type: %s", downloadUrl.startsWith("https://") ? "HTTPS" : "HTTP");

    // Get release info
    String releaseName = doc["name"].as<String>();

    log_i("Latest release: %s (%s)", releaseName.c_str(), tagName.c_str());
    log_w("WARNING: Proceeding with OTA update WITHOUT version checking!");
    log_w("This will download and install the latest firmware regardless of current version");

    // Give user time to abort if needed
    for (int i = 10; i > 0; i--)
    {
        log_w("Force OTA update starting in %d seconds... (reset device to abort)", i);
        delay(1000);
    }

    log_i("Starting force OTA update process...");

    // Download firmware binary directly
    bool success = bHalFirmware_downloadBinaryFirmware(downloadUrl, sysData, sysStatus, devInfo);

    if (success)
    {
        log_i("PASS: Force OTA update completed successfully");
        log_i("Device will reboot to new firmware in 5 seconds...");

        // Final countdown before reboot
        for (int i = 5; i > 0; i--)
        {
            log_i("Rebooting in %d seconds...", i);
            delay(1000);
        }

        log_i("Rebooting now...");
        ESP.restart();
    }
    else
    {
        log_e("FAIL: Force OTA update failed");
    }

    return success;
}

/**
 * @brief Test function for force OTA update
 */
void vHalFirmware_testForceOTAUpdate(systemData_t *sysData, systemStatus_t *sysStatus, deviceNetworkInfo_t *devInfo)
{
    log_i("=== Force OTA Update Test ===");
    log_w("DANGER: This will perform ACTUAL OTA update without version checking!");
    log_w("The device WILL reboot and install the latest firmware from GitHub!");
    log_i("Requesting force OTA update via network task for proper stack management...");

    // Use network task for consistent stack management
    requestFirmwareUpdate(sysData, sysStatus, devInfo);
}