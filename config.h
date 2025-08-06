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

// API Server Configuration
#ifndef API_SERVER
#define API_SERVER "milanosmartpark.info"
#endif

// API Security Configuration
#ifndef API_SECRET_SALT
#define API_SECRET_SALT "2198be9d8e83a662210ef9cd8acc5b36"
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