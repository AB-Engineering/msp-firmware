/********************************************************
 * @file    config.h
 * @author  AB-Engineering - https://ab-engineering.it
 * @brief   Configuration constants for the Milano Smart Park project
 * @details Centralized configuration management for API credentials, pins, and hardware settings
 * @version 0.1
 * @date    2025-08-05
 * 
 * @copyright Copyright (c) 2025
 * 
 ********************************************************/

#ifndef CONFIG_H
#define CONFIG_H

// ===== Network Configuration =====

// #define ENABLE_FIRMWARE_UPDATE_TESTS      // Re-enabled: for controlled testing
// #define DISABLE_AUTOMATIC_FIRMWARE_TESTS  // Critical: no auto-testing
// #define ENABLE_ACTUAL_OTA_UPDATE_TEST     // Re-enabled: for controlled testing
// #define ENABLE_FOTA_MODE               // Disabled: missing task implementation causing boot loop
// #define ENABLE_FORCE_OTA_UPDATE_TEST   // Too risky

// API Server Configuration
#ifndef API_SERVER
#define __API_SERVER "server.info"
#else
#define __API_SERVER API_SERVER
#endif

// API Security Configuration
#ifndef API_SECRET_SALT
#define __API_SECRET_SALT "secret_salt"
#else
#define __API_SECRET_SALT API_SECRET_SALT
#endif

// ===== Hardware Pin Definitions =====

// Sensor Pin Definitions
#define PMSERIAL_RX 14
#define PMSERIAL_TX 12

// Network Hardware Pin Definitions
#define MODEM_RST 4
#define SSL_RAND_PIN 35

// Network Hardware Configuration
#define MODEM_TX 13
#define MODEM_RX 15

// ===== Network Protocol Configuration =====

// NTP Configuration
#define NTP_SERVER_DEFAULT "pool.ntp.org"
#define TZ_DEFAULT "GMT0"

// Connection Timeouts
#define WIFI_CONNECTION_TIMEOUT_MS 15000
#define GPRS_CONNECTION_TIMEOUT_MS 30000
#define SERVER_RESPONSE_TIMEOUT_MS 10000

// Retry Configuration
#define MAX_CONNECTION_RETRIES 3
#define NETWORK_RETRY_DELAY_MS 5000

// Data Transmission Configuration
#define NTP_SYNC_TX_COUNT 100
#define SEND_DATA_TIMEOUT_IN_SEC (5 * 60)  // 5 minutes

// ===== Version Information =====

#ifndef VERSION_STRING
#define VERSION_STRING "DEV"
#endif

#endif // CONFIG_H