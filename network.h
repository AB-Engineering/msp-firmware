/**************************************************************************
 * @file    network.h (Improved Version)
 * @author  Refactored by AB-Engineering - https://ab-engineering.it
 * @brief   Enhanced network management functions for the Milano Smart Park project
 * @details Thread-safe network management with proper state machine implementation
 * @version 0.2
 * @date    2025-08-05
 *
 * @copyright Copyright (c) 2025
 *
 ************************************************************************/

#ifndef NETWORK_H
#define NETWORK_H

// --includes
#include "shared_values.h"
#include "SSLClient.h"
#include <WiFi.h>
#include <stdbool.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>

// Default configuration values
#define NTP_SERVER_DEFAULT "pool.ntp.org"
#define TZ_DEFAULT "GMT0"

// Network internal event bits for task management (different from public events)
#define NET_EVT_CONNECT_REQ      (1 << 5)
#define NET_EVT_DISCONNECT_REQ   (1 << 6)
#define NET_EVT_TIME_SYNC_REQ    (1 << 7)
#define NET_EVT_DATA_READY       (1 << 8)
#define NET_EVT_FW_UPDATE_REQ    (1 << 9)
#define NET_EVT_CONFIG_UPDATED   (1 << 10)

// Network task states
typedef enum __NETWORK_TASK_EVT__
{
    NETWRK_EVT_WAIT = 0,                /*!< Wait for events or periodic maintenance */
    NETWRK_EVT_INIT_CONNECTION,         /*!< Initialize network connection (WiFi or GSM) */
    NETWRK_EVT_SYNC_DATETIME,          /*!< Synchronize date and time via NTP */
    NETWRK_EVT_UPDATE_DATA,            /*!< Process and send queued data to server */
    NETWRK_EVT_DEINIT_CONNECTION,      /*!< Deinitialize network connections */
    NETWRK_EVT_FW_UPDATE_CHECK,        /*!< Check for firmware updates */
    //--- LAST EVENT
    NETWRK_EVT_MAX_EVENTS              /*!< Maximum number of events (keep last) */
} netwkr_task_evt_t;

// Network connection types
typedef enum {
    NETWORK_TYPE_NONE = 0,
    NETWORK_TYPE_WIFI,
    NETWORK_TYPE_GSM,
    NETWORK_TYPE_BOTH
} network_type_t;

// Network status structure
typedef struct {
    bool wifiConnected;
    bool gsmConnected;
    bool timeSync;
    int connectionRetries;
    unsigned long lastConnectionAttempt;
    network_type_t activeConnection;
} network_status_t;

#ifdef __cplusplus
extern "C" {
#endif

// ===== Core Network Management Functions =====

/**
 * @brief Initialize the network subsystem and create the network task
 * @details This function must be called before using any other network functions.
 *          It creates the data queue, event group, mutex, and network task.
 * @param sysData Pointer to initialized system data structure
 * @param sysStatus Pointer to system status structure
 * @param devInfo Pointer to device network info structure
 */
void initSendDataOp(systemData_t *sysData, systemStatus_t *sysStatus, deviceNetworkInfo_t *devInfo);

/**
 * @brief Create network event group for inter-task communication
 * @details Creates the event group used for signaling between tasks
 */
void createNetworkEvents(void);

// ===== Data Queue Management =====

/**
 * @brief Enqueue data for transmission to server (C-compatible wrapper)
 * @param data Pointer to the data structure to send
 * @param ticksToWait Maximum time to wait if queue is full
 * @return true if data was successfully enqueued, false otherwise
 */
bool enqueueSendData_C(const send_data_t *data, TickType_t ticksToWait);

#ifdef __cplusplus
/**
 * @brief Enqueue data for transmission to server (C++ version)
 * @param data Reference to the data structure to send
 * @param ticksToWait Maximum time to wait if queue is full
 * @return true if data was successfully enqueued, false otherwise
 */
bool enqueueSendData(const send_data_t &data, TickType_t ticksToWait);
#else
// In C, map the C++ function to the C wrapper
#define enqueueSendData(data, timeout) enqueueSendData_C(&(data), timeout)
#endif

/**
 * @brief Dequeue data from transmission queue (internal use)
 * @param data Pointer to store the dequeued data
 * @param ticksToWait Maximum time to wait if queue is empty
 * @return true if data was successfully dequeued, false otherwise
 */
bool dequeueSendData(send_data_t *data, TickType_t ticksToWait);

// ===== Event Management =====

/**
 * @brief Send a network event to the network task
 * @param event Event bit to set
 * @return true if event was successfully sent, false otherwise
 */
bool sendNetworkEvent(net_evt_t event);

/**
 * @brief Check if a specific network event is currently set
 * @param event Event bit to check
 * @return true if event is set, false otherwise
 */
bool checkNetworkEvent(net_evt_t event);

/**
 * @brief Wait for a specific network event to occur
 * @param event Event bit to wait for
 * @param ticksToWait Maximum time to wait for the event
 * @return true if event occurred within timeout, false otherwise
 */
bool waitForNetworkEvent(net_evt_t event, TickType_t ticksToWait);

// ===== Network Connection Management =====

/**
 * @brief Request network connection establishment
 * @details Signals the network task to establish connection (WiFi or GSM)
 */
void requestNetworkConnection(void);

/**
 * @brief Request network disconnection
 * @details Signals the network task to disconnect from all networks
 */
void requestNetworkDisconnection(void);

/**
 * @brief Request time synchronization
 * @details Signals the network task to sync time via NTP
 */
void requestTimeSync(void);

/**
 * @brief Update network configuration from SD card
 * @details Signals the network task to reload configuration from SD card
 */
void updateNetworkConfig(void);

/**
 * @brief Get current network status
 * @param wifiConnected Pointer to store WiFi connection status
 * @param gsmConnected Pointer to store GSM connection status
 * @param timeSync Pointer to store time sync status
 * @return true if status was successfully retrieved, false otherwise
 */
bool getNetworkStatus(bool *wifiConnected, bool *gsmConnected, bool *timeSync);

/**
 * @brief Check if network task is running
 * @return true if network task is active, false otherwise
 */
bool isNetworkTaskRunning(void);

// ===== Network Configuration Functions =====

/**
 * @brief Set default SSL server name and mark server as OK
 * @param p_tData Pointer to system data structure
 */
void vMspInit_setDefaultSslName(systemData_t *p_tData);

/**
 * @brief Set API security salt and firmware version
 * @param p_tData Pointer to system data structure
 */
void vMspInit_setApiSecSaltAndFwVer(systemData_t *p_tData);

// ===== Hardware Access Functions =====

/**
 * @brief Get the GSM SSL client instance
 * @return Pointer to SSLClient instance, or nullptr if not available
 */
SSLClient *tHalNetwork_getGSMClient(void);

/**
 * @brief Disconnect from GSM modem
 * @return 1 if successful, 0 if failed
 */
uint8_t vHalNetwork_modemDisconnect(void);

/**
 * @brief Print WiFi MAC address and update display
 * @param p_tSys Pointer to system status structure
 * @param p_tDev Pointer to device network info structure
 */
void vHalNetwork_printWiFiMACAddr(systemStatus_t *p_tSys, deviceNetworkInfo_t *p_tDev);

// ===== Legacy Compatibility Functions =====
// These functions maintain backward compatibility but internally use the new task-based system

/**
 * @brief Connect to internet and retrieve current date/time (Legacy)
 * @param p_tSys Pointer to system status structure
 * @param p_tDev Pointer to device network info structure
 * @param p_tSysData Pointer to system data structure
 * @param p_tm Pointer to time structure to fill
 * @deprecated Use requestNetworkConnection() and requestTimeSync() instead
 */
void vHalNetwork_connAndGetTime(systemStatus_t *p_tSys, deviceNetworkInfo_t *p_tDev, 
                               systemData_t *p_tSysData, tm *p_tm);

/**
 * @brief Connect to internet using WiFi or GPRS (Legacy)
 * @param p_tSys Pointer to system status structure
 * @param p_tDev Pointer to device network info structure
 * @deprecated Use requestNetworkConnection() instead
 */
void vHalNetwork_connectToInternet(systemStatus_t *p_tSys, deviceNetworkInfo_t *p_tDev);

/**
 * @brief Connect to server and send data (Legacy)
 * @param p_tClient Pointer to SSL client (unused in new implementation)
 * @param p_tDataToSent Pointer to data to send
 * @param p_tDev Pointer to device network info structure
 * @param p_tData Pointer to sensor data structure
 * @param p_tSysData Pointer to system data structure
 * @deprecated Use enqueueSendData() instead
 */
void vHalNetwork_connectToServer(SSLClient *p_tClient, send_data_t *p_tDataToSent, 
                                deviceNetworkInfo_t *p_tDev, sensorData_t *p_tData, 
                                systemData_t *p_tSysData);

#ifdef __cplusplus
}
#endif

// ===== Configuration Macros =====

// Network task configuration
#ifndef NETWORK_TASK_STACK_SIZE
#define NETWORK_TASK_STACK_SIZE (8 * 1024)
#endif

#ifndef NETWORK_TASK_PRIORITY
#define NETWORK_TASK_PRIORITY 1
#endif

#ifndef SEND_DATA_QUEUE_LENGTH
#define SEND_DATA_QUEUE_LENGTH 4
#endif

// Connection timeout configuration
#ifndef WIFI_CONNECTION_TIMEOUT_MS
#define WIFI_CONNECTION_TIMEOUT_MS 15000
#endif

#ifndef GPRS_CONNECTION_TIMEOUT_MS
#define GPRS_CONNECTION_TIMEOUT_MS 30000
#endif

#ifndef SERVER_RESPONSE_TIMEOUT_MS
#define SERVER_RESPONSE_TIMEOUT_MS 10000
#endif

// Retry configuration
#ifndef MAX_CONNECTION_RETRIES
#define MAX_CONNECTION_RETRIES 3
#endif

#ifndef NETWORK_RETRY_DELAY_MS
#define NETWORK_RETRY_DELAY_MS 5000
#endif

// Debug configuration
#ifdef NETWORK_DEBUG
#define NETWORK_LOG_LEVEL ESP_LOG_DEBUG
#else
#define NETWORK_LOG_LEVEL ESP_LOG_INFO
#endif

// Version information
#define NETWORK_LIB_VERSION_MAJOR 0
#define NETWORK_LIB_VERSION_MINOR 2
#define NETWORK_LIB_VERSION_PATCH 0
#define NETWORK_LIB_VERSION "0.2.0"

#endif // NETWORK_H

//************************************** EOF **************************************