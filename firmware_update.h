/************************************************************************************************
 * @file    firmware_update.h
 * @author  AB-Engineering - https://ab-engineering.it
 * @brief   Firmware update management for the Milano Smart Park project
 * @version 0.1
 * @date    2025-01-11
 *
 * @copyright Copyright (c) 2025
 *
 ************************************************************************************************/

#ifndef FIRMWARE_UPDATE_H
#define FIRMWARE_UPDATE_H

#include <Arduino.h>
#include "shared_values.h"

// Function declarations
void vHalFirmware_checkForUpdates(systemData_t *sysData, systemStatus_t *sysStatus, deviceNetworkInfo_t *devInfo);
bool bHalFirmware_compareVersions(const String &currentVersion, const String &remoteVersion);
bool bHalFirmware_downloadBinaryFirmware(const String &downloadUrl, systemData_t *sysData, systemStatus_t *sysStatus, deviceNetworkInfo_t *devInfo);
bool bHalFirmware_performOTAUpdate(const String &firmwarePath);
bool bHalFirmware_forceOTAUpdate(systemData_t *sysData, systemStatus_t *sysStatus, deviceNetworkInfo_t *devInfo);

// ESP-IDF OTA management functions
void vHalFirmware_printOTAInfo();
bool bHalFirmware_validateCurrentFirmware();
bool bHalFirmware_markFirmwareValid();
bool bHalFirmware_rollbackFirmware();
bool bHalFirmware_isRollbackAvailable();

// Test functions
void vHalFirmware_testVersionComparison();
void vHalFirmware_testGitHubAPI();
void vHalFirmware_testConfigParsing(systemStatus_t *sysStatus);
void vHalFirmware_testOTAManagement();
void vHalFirmware_testForceOTAUpdate(systemData_t *sysData, systemStatus_t *sysStatus, deviceNetworkInfo_t *devInfo);

#ifdef ENABLE_FOTA_MODE
// FOTA mode functions for task management and memory optimization
void vHalFirmware_enterFOTAMode();
void vHalFirmware_exitFOTAMode();
bool bHalFirmware_performFOTAInMainLoop(systemData_t *sysData, systemStatus_t *sysStatus, deviceNetworkInfo_t *devInfo);
#endif

#endif // FIRMWARE_UPDATE_H