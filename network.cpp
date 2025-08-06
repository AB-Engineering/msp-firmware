/****************************************************
 * @file    network.cpp (Improved Version)
 * @author  AB-Engineering - https://ab-engineering.it
 * @brief   Enhanced Network management for the Milano Smart Park project
 * @details Thread-safe network management with proper state machine implementation
 * @version 0.2
 * @date    2025-08-05
 *
 * @copyright Copyright (c) 2025
 *
 ****************************************************/

#define TINY_GSM_MODEM_SIM800

#include <Arduino.h>
#include <TinyGsmClient.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "network.h"
#include "trust_anchor.h"
#include "display_task.h"
#include "mspOs.h"
#include "sdcard.h"
#include "sensors.h"
#include "config.h"

// -- Network Configuration Constants
#define TIME_SYNC_MAX_RETRY 5

// Modem buffer configuration
#if !defined(TINY_GSM_RX_BUFFER)
#define TINY_GSM_RX_BUFFER 650
#endif

// Network hardware objects (moved from main file)
static WiFiClient wifi_base;
static SSLClient wificlient(wifi_base, TAs, (size_t)TAs_NUM, SSL_RAND_PIN, 1, SSLClient::SSL_ERROR);

// Hardware serial for GSM
HardwareSerial gsmSerial(1);

// Task configuration
#define NETWORK_TASK_STACK_SIZE (8 * 1024)
#define NETWORK_TASK_PRIORITY 1
#define SEND_DATA_QUEUE_LENGTH 4

// Static task variables
static StaticTask_t networkTaskBuffer;
static StackType_t networkTaskStack[NETWORK_TASK_STACK_SIZE];
static TaskHandle_t networkTaskHandle = NULL;

// Queue and synchronization objects
static QueueHandle_t sendDataQueue = NULL;
static EventGroupHandle_t networkEventGroup = NULL;
static StaticEventGroup_t networkEventGroupBuffer;
static SemaphoreHandle_t networkStateMutex = NULL;
static StaticSemaphore_t networkStateMutexBuffer;

// Network state variables (protected by mutex)
static struct {
    bool wifiConnected;
    bool gsmConnected;
    bool timeSync;
    int connectionRetries;
    unsigned long lastConnectionAttempt;
    netwkr_task_evt_t currentState;
    netwkr_task_evt_t nextState;
    bool taskRunning;
    bool configurationLoaded;
    int ntpSyncExpired;  // Counter for NTP sync expiration
} networkState = {
    .wifiConnected = false,
    .gsmConnected = false,
    .timeSync = false,
    .connectionRetries = 0,
    .lastConnectionAttempt = 0,
    .currentState = NETWRK_EVT_WAIT,
    .nextState = NETWRK_EVT_WAIT,
    .taskRunning = false,
    .configurationLoaded = false,
    .ntpSyncExpired = NTP_SYNC_TX_COUNT  // Initialize with default count
};

// Global instances (properly managed within task)
static TinyGsm* modem = NULL;
static TinyGsmClient* gsmClient = NULL;
static SSLClient* sslClient = NULL;

// Display data for network events
static displayData_t displayData;

// Global data structure pointers (shared with main task)
static systemData_t *globalSysData = NULL;
static systemStatus_t *globalSysStatus = NULL;
static deviceNetworkInfo_t *globalDevInfo = NULL;

// Forward declarations
static void networkTask(void *pvParameters);
static bool initializeNetworkResources();
static void cleanupNetworkResources();
static bool handleWiFiConnection(deviceNetworkInfo_t *devInfo, systemStatus_t *sysStatus);
static bool handleGSMConnection(deviceNetworkInfo_t *devInfo, systemStatus_t *sysStatus);
static bool syncDateTime(deviceNetworkInfo_t *devInfo, systemStatus_t *sysStatus, systemData_t *sysData);
static bool sendDataToServer(send_data_t *dataToSend, deviceNetworkInfo_t *devInfo, 
                           systemStatus_t *sysStatus, systemData_t *sysData);
static void updateNetworkState(netwkr_task_evt_t newState);
static netwkr_task_evt_t getNetworkState();
static void updateDisplayStatus(deviceNetworkInfo_t *devInfo, systemStatus_t *sysStatus, displayEvents_t event);
static bool isNetworkConnected();
static void handleNetworkError(const char* error, displayEvents_t displayEvent);
static bool loadNetworkConfiguration(deviceNetworkInfo_t *devInfo, systemStatus_t *sysStatus, 
                                   systemData_t *sysData, sensorData_t *sensorData, 
                                   deviceMeasurement_t *measStat);

// Public interface functions
bool enqueueSendData(const send_data_t &data, TickType_t ticksToWait)
{
    if (sendDataQueue == NULL) {
        log_e("Send data queue not initialized");
        return false;
    }
    
    BaseType_t result = xQueueSend(sendDataQueue, &data, ticksToWait);
    if (result != pdPASS) {
        log_w("Failed to enqueue send data, queue might be full");
        return false;
    }
    
    // Trigger network task to process data  
    xEventGroupSetBits(networkEventGroup, NET_EVT_DATA_READY);
    return true;
}

// C-compatible wrapper function
bool enqueueSendData_C(const send_data_t *data, TickType_t ticksToWait)
{
    if (!data) {
        log_e("Invalid data pointer");
        return false;
    }
    return enqueueSendData(*data, ticksToWait);
}

bool dequeueSendData(send_data_t *data, TickType_t ticksToWait)
{
    if (sendDataQueue == NULL || data == NULL) {
        log_e("Invalid parameters for dequeue operation");
        return false;
    }
    
    return (xQueueReceive(sendDataQueue, data, ticksToWait) == pdPASS);
}

void initSendDataOp(systemData_t *sysData, systemStatus_t *sysStatus, deviceNetworkInfo_t *devInfo)
{
    // Store global data structure pointers
    globalSysData = sysData;
    globalSysStatus = sysStatus;
    globalDevInfo = devInfo;
    
    log_i("Network task initialized with global data structures");
    log_i("Server from main task: %s", globalSysData->server.c_str());
    log_i("Server OK from main task: %d", globalSysStatus->server_ok);
    log_i("API secret salt: %s", globalSysData->api_secret_salt.c_str());
    
    // Create queue
    if (sendDataQueue == NULL) {
        sendDataQueue = xQueueCreate(SEND_DATA_QUEUE_LENGTH, sizeof(send_data_t));
        if (sendDataQueue == NULL) {
            log_e("Failed to create send data queue");
            return;
        }
        log_i("Send data queue created successfully");
    }

    // Create mutex for network state protection
    if (networkStateMutex == NULL) {
        networkStateMutex = xSemaphoreCreateMutexStatic(&networkStateMutexBuffer);
        if (networkStateMutex == NULL) {
            log_e("Failed to create network state mutex");
            return;
        }
        log_i("Network state mutex created successfully");
    }

    // Create network task
    if (networkTaskHandle == NULL) {
        networkTaskHandle = xTaskCreateStaticPinnedToCore(
            networkTask,
            "NetworkTask",
            NETWORK_TASK_STACK_SIZE,
            NULL,
            NETWORK_TASK_PRIORITY,
            networkTaskStack,
            &networkTaskBuffer,
            1  // Core 1
        );
        
        if (networkTaskHandle == NULL) {
            log_e("Failed to create network task");
        } else {
            log_i("Network task created successfully");
            // Mark task as running
            if (xSemaphoreTake(networkStateMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                networkState.taskRunning = true;
                xSemaphoreGive(networkStateMutex);
            }
        }
    }
}

void createNetworkEvents()
{
    if (networkEventGroup == NULL) {
        networkEventGroup = xEventGroupCreateStatic(&networkEventGroupBuffer);
        if (networkEventGroup == NULL) {
            log_e("Failed to create network event group");
        } else {
            log_i("Network event group created successfully");
        }
    }
}

bool sendNetworkEvent(net_evt_t event)
{
    if (networkEventGroup == NULL) {
        log_e("Network event group not initialized");
        return false;
    }

    EventBits_t result = xEventGroupSetBits(networkEventGroup, event);
    return (result & event) != 0;
}

bool checkNetworkEvent(net_evt_t event)
{
    if (networkEventGroup == NULL) {
        return false;
    }
    
    EventBits_t currentBits = xEventGroupGetBits(networkEventGroup);
    return (currentBits & event) != 0;
}

bool waitForNetworkEvent(net_evt_t event, TickType_t ticksToWait)
{
    if (networkEventGroup == NULL) {
        log_e("Network event group not initialized");
        return false;
    }

    EventBits_t result = xEventGroupWaitBits(
        networkEventGroup, 
        event, 
        pdTRUE,  // Clear bits on exit
        pdFALSE, // Wait for any bit
        ticksToWait
    );
    
    return (result & event) != 0;
}

SSLClient* tHalNetwork_getGSMClient()
{
    return sslClient;
}

uint8_t vHalNetwork_modemDisconnect()
{
    if (modem && modem->isGprsConnected()) {
        log_i("Disconnecting from GPRS...");
        bool result = modem->gprsDisconnect();
        
        // Update state
        if (xSemaphoreTake(networkStateMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            networkState.gsmConnected = false;
            xSemaphoreGive(networkStateMutex);
        }
        
        return result ? 1 : 0;
    }
    return 1; // Already disconnected
}

// Network state management (thread-safe)
static void updateNetworkState(netwkr_task_evt_t newState)
{
    // No mutex needed - only called from within network task
    networkState.nextState = newState;
}

static netwkr_task_evt_t getNetworkState()
{
    // No mutex needed - only called from within network task
    return networkState.currentState;
}

static bool isNetworkConnected()
{
    // No mutex needed - only called from within network task
    return networkState.wifiConnected || networkState.gsmConnected;
}

static void handleNetworkError(const char* error, displayEvents_t displayEvent)
{
    log_e("Network error: %s", error);
    
    // Update retry counter - no mutex needed (internal state)
    networkState.connectionRetries++;
    networkState.lastConnectionAttempt = millis();
    
    // Update display if needed - check for valid display event
    // Note: We could define a DISP_EVENT_MAX_EVENTS or just check for reasonable range
    if (displayEvent >= 0) {
        deviceNetworkInfo_t devInfo = {};
        systemStatus_t sysStatus = {};
        updateDisplayStatus(&devInfo, &sysStatus, displayEvent);
    }
}

// Initialize network resources
static bool initializeNetworkResources()
{
    log_i("Initializing network resources...");
    
    // Initialize GSM serial
    gsmSerial.begin(9600, SERIAL_8N1, MODEM_RX, MODEM_TX);
    delay(1000);
    
    // Create modem instance
    if (modem == NULL) {
        modem = new TinyGsm(gsmSerial);
        if (modem == NULL) {
            log_e("Failed to create TinyGsm instance");
            return false;
        }
    }
    
    // Create GSM client
    if (gsmClient == NULL) {
        gsmClient = new TinyGsmClient(*modem);
        if (gsmClient == NULL) {
            log_e("Failed to create TinyGsmClient instance");
            delete modem;
            modem = NULL;
            return false;
        }
    }
    
    // Create SSL client
    if (sslClient == NULL) {
        sslClient = new SSLClient(*gsmClient, TAs, (size_t)TAs_NUM, SSL_RAND_PIN, 1, SSLClient::SSL_ERROR);
        if (sslClient == NULL) {
            log_e("Failed to create SSLClient instance");
            delete gsmClient;
            delete modem;
            gsmClient = NULL;
            modem = NULL;
            return false;
        }
    }
    
    log_i("Network resources initialized successfully");
    return true;
}

// Cleanup network resources
static void cleanupNetworkResources()
{
    log_i("Cleaning up network resources...");
    
    // Disconnect WiFi
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.disconnect();
        WiFi.mode(WIFI_OFF);
        log_i("WiFi disconnected and turned off");
    }
    
    // Disconnect GSM
    if (modem && modem->isGprsConnected()) {
        modem->gprsDisconnect();
        log_i("GPRS disconnected");
    }
    
    // Clean up SSL client
    if (sslClient) {
        sslClient->stop();
        delete sslClient;
        sslClient = NULL;
        log_d("SSLClient cleaned up");
    }
    
    // Clean up GSM client
    if (gsmClient) {
        delete gsmClient;
        gsmClient = NULL;
        log_d("GSMClient cleaned up");
    }
    
    // Clean up modem
    if (modem) {
        delete modem;
        modem = NULL;
        log_d("Modem cleaned up");
    }
    
    // Update state
    if (xSemaphoreTake(networkStateMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        networkState.wifiConnected = false;
        networkState.gsmConnected = false;
        networkState.timeSync = false;
        networkState.connectionRetries = 0;
        xSemaphoreGive(networkStateMutex);
    }
    
    log_i("Network resources cleaned up successfully");
}

// WiFi connection handler
static bool handleWiFiConnection(deviceNetworkInfo_t *devInfo, systemStatus_t *sysStatus)
{
    log_i("Attempting WiFi connection to SSID: %s", devInfo->ssid.c_str());
    
    // Validate input parameters
    if (devInfo->ssid.length() == 0) {
        log_e("WiFi SSID is empty");
        return false;
    }
    
    // Set WiFi mode and power
    WiFi.mode(WIFI_STA);
    delay(1000);
    WiFi.setTxPower(devInfo->wifipow);
    log_i("WiFi power set to %d", devInfo->wifipow);
    
    updateDisplayStatus(devInfo, sysStatus, DISP_EVENT_CONN_TO_WIFI);
    
    for (int retry = 0; retry < MAX_CONNECTION_RETRIES; retry++) {
        log_i("WiFi connection attempt %d/%d", retry + 1, MAX_CONNECTION_RETRIES);
        
        // Scan for networks
        int networks = WiFi.scanNetworks();
        if (networks <= 0) {
            log_w("No networks found on attempt %d", retry + 1);
            updateDisplayStatus(devInfo, sysStatus, DISP_EVENT_NO_NETWORKS_FOUND);
            
            if (retry < MAX_CONNECTION_RETRIES - 1) {
                delay(NETWORK_RETRY_DELAY_MS);
            }
            continue;
        }
        
        log_i("Found %d networks", networks);
        
        // Look for target SSID
        bool ssidFound = false;
        int targetRSSI = -999;
        
        for (int i = 0; i < networks; i++) {
            String currentSSID = WiFi.SSID(i);
            int currentRSSI = WiFi.RSSI(i);
            
            log_v("Network %d: %s (RSSI: %d)", i, currentSSID.c_str(), currentRSSI);
            
            if (currentSSID == devInfo->ssid) {
                ssidFound = true;
                targetRSSI = currentRSSI;
                break;
            }
        }
        
        if (!ssidFound) {
            log_w("SSID '%s' not found in scan", devInfo->ssid.c_str());
            devInfo->noNet = "NO " + devInfo->ssid + "!";
            updateDisplayStatus(devInfo, sysStatus, DISP_EVENT_SSID_NOT_FOUND);
            
            if (retry < MAX_CONNECTION_RETRIES - 1) {
                delay(NETWORK_RETRY_DELAY_MS);
            }
            continue;
        }
        
        log_i("Target SSID found with RSSI: %d", targetRSSI);
        devInfo->foundNet = devInfo->ssid + " OK!";
        
        // Attempt connection
        WiFi.begin(devInfo->ssid.c_str(), devInfo->passw.c_str());
        
        unsigned long startTime = millis();
        wl_status_t wifiStatus = WL_IDLE_STATUS;
        
        while ((wifiStatus = WiFi.status()) != WL_CONNECTED && 
               (millis() - startTime) < WIFI_CONNECTION_TIMEOUT_MS) {
            delay(500);
            
            // Check for connection failures
            if (wifiStatus == WL_CONNECT_FAILED || wifiStatus == WL_CONNECTION_LOST) {
                log_w("WiFi connection failed with status: %d", wifiStatus);
                break;
            }
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            log_i("WiFi connected successfully");
            log_i("IP address: %s", WiFi.localIP().toString().c_str());
            log_i("Gateway: %s", WiFi.gatewayIP().toString().c_str());
            log_i("DNS: %s", WiFi.dnsIP().toString().c_str());
            
            // Update state - no mutex needed (internal state)
            networkState.wifiConnected = true;
            networkState.connectionRetries = 0;
            
            sysStatus->connection = true;
            sendNetworkEvent(NET_EVENT_CONNECTED);
            return true;
        }
        
        log_w("WiFi connection failed on attempt %d (Status: %d)", retry + 1, WiFi.status());
        WiFi.disconnect();
        
        if (retry < MAX_CONNECTION_RETRIES - 1) {
            devInfo->remain = String(MAX_CONNECTION_RETRIES - retry - 1) + " tries remain.";
            updateDisplayStatus(devInfo, sysStatus, DISP_EVENT_CONN_RETRY);
            delay(NETWORK_RETRY_DELAY_MS);
        }
    }
    
    log_e("WiFi connection failed after all retries");
    updateDisplayStatus(devInfo, sysStatus, DISP_EVENT_WIFI_DISCONNECTED);
    sysStatus->connection = false;
    
    // Update retry count - no mutex needed (internal state)
    networkState.connectionRetries++;
    
    return false;
}

// GSM connection handler
static bool handleGSMConnection(deviceNetworkInfo_t *devInfo, systemStatus_t *sysStatus)
{
    log_i("Attempting GSM connection with APN: %s", devInfo->apn.c_str());
    updateDisplayStatus(devInfo, sysStatus, DISP_EVENT_CONN_TO_GPRS);
    
    if (!modem) {
        log_e("Modem not initialized");
        return false;
    }
    
    // Validate APN
    if (devInfo->apn.length() == 0) {
        log_e("APN is empty");
        return false;
    }
    
    // Initialize/restart modem
    log_i("Initializing modem...");
    modem->restart();
    delay(3000);  // Give modem time to initialize
    
    // Get modem information
    String modelName = modem->getModemName();
    String modemInfo = modem->getModemInfo();
    String imei = modem->getIMEI();
    
    log_i("Modem: %s", modelName.c_str());
    log_i("Info: %s", modemInfo.c_str());
    log_i("IMEI: %s", imei.c_str());
    
    // Check SIM card
    String ccid = modem->getSimCCID();
    String imsi = modem->getIMSI();
    
    if (ccid.startsWith("ERROR") || imsi.startsWith("ERROR") || 
        ccid.length() < 10 || imsi.length() < 10) {
        log_e("SIM card error - CCID: %s, IMSI: %s", ccid.c_str(), imsi.c_str());
        updateDisplayStatus(devInfo, sysStatus, DISP_EVENT_SIM_ERROR);
        return false;
    }
    
    log_i("SIM card detected - CCID: %s, IMSI: %s", ccid.c_str(), imsi.c_str());
    
    // Wait for cellular network
    log_i("Waiting for cellular network...");
    unsigned long networkStart = millis();
    bool networkFound = false;
    
    while (!networkFound && (millis() - networkStart) < GPRS_CONNECTION_TIMEOUT_MS) {
        networkFound = modem->waitForNetwork(5000);
        if (!networkFound) {
            log_w("Still waiting for network... (%lu ms elapsed)", 
                  millis() - networkStart);
            delay(2000);
        }
    }
    
    if (!modem->isNetworkConnected()) {
        log_e("Failed to connect to cellular network after timeout");
        updateDisplayStatus(devInfo, sysStatus, DISP_EVENT_NETWORK_ERROR);
        return false;
    }
    
    // Get network information
    String operatorName = modem->getOperator();
    int signalQuality = modem->getSignalQuality();
    
    log_i("Cellular network connected");
    log_i("Operator: %s", operatorName.c_str());
    log_i("Signal quality: %d", signalQuality);
    
    if (signalQuality < 5) {
        log_w("Low signal quality detected: %d", signalQuality);
    }
    
    // Connect to GPRS
    log_i("Connecting to GPRS with APN: %s", devInfo->apn.c_str());
    
    for (int retry = 0; retry < MAX_CONNECTION_RETRIES; retry++) {
        log_i("GPRS connection attempt %d/%d", retry + 1, MAX_CONNECTION_RETRIES);
        
        if (modem->gprsConnect(devInfo->apn.c_str(), "", "")) {
            // Verify connection
            IPAddress localIP = modem->localIP();
            log_i("GPRS connected successfully");
            log_i("Local IP: %s", localIP.toString().c_str());
            
            // Update state - no mutex needed (internal state)
            networkState.gsmConnected = true;
            networkState.connectionRetries = 0;
            
            sysStatus->connection = true;
            sendNetworkEvent(NET_EVENT_CONNECTED);
            return true;
        }
        
        log_w("GPRS connection failed, attempt %d/%d", retry + 1, MAX_CONNECTION_RETRIES);
        
        if (retry < MAX_CONNECTION_RETRIES - 1) {
            log_i("Retrying GPRS connection in %d ms...", NETWORK_RETRY_DELAY_MS);
            delay(NETWORK_RETRY_DELAY_MS);
        }
    }
    
    log_e("GPRS connection failed after all retries");
    updateDisplayStatus(devInfo, sysStatus, DISP_EVENT_GPRS_ERROR);
    sysStatus->connection = false;
    
    // Update retry count - no mutex needed (internal state)
    networkState.connectionRetries++;
    
    return false;
}

// DateTime synchronization
static bool syncDateTime(deviceNetworkInfo_t *devInfo, systemStatus_t *sysStatus, systemData_t *sysData)
{
    log_i("Synchronizing date and time...");
    updateDisplayStatus(devInfo, sysStatus, DISP_EVENT_RETREIVE_DATETIME);
    
    // Configure timezone
    String tzRule = (sysData->timezone.length() > 0) ? sysData->timezone : TZ_DEFAULT;
    log_i("Setting timezone: %s", tzRule.c_str());
    setenv("TZ", tzRule.c_str(), 1);
    tzset();
    
    // Validate NTP server
    String ntpServer = (sysData->ntp_server.length() > 0) ? sysData->ntp_server : NTP_SERVER_DEFAULT;
    log_i("Using NTP server: %s", ntpServer.c_str());
    
    struct tm timeInfo;
    bool timeObtained = false;
    
    for (int retry = 0; retry < TIME_SYNC_MAX_RETRY && !timeObtained; retry++) {
        log_i("Time sync attempt %d/%d", retry + 1, TIME_SYNC_MAX_RETRY);
        
        if (sysStatus->use_modem && modem && modem->isGprsConnected()) {
            // Use GSM time sync
            log_i("Syncing time via GSM modem...");
            
            int year, month, day, hour, minute, second;
            float timezone;
            
            // Try NTP sync via modem
            if (modem->NTPServerSync(ntpServer, 0)) {
                delay(2000);  // Wait for sync to complete
                
                if (modem->getNetworkTime(&year, &month, &day, &hour, &minute, &second, &timezone)) {
                    timeInfo.tm_year = year - 1900;
                    timeInfo.tm_mon = month - 1;
                    timeInfo.tm_mday = day;
                    timeInfo.tm_hour = hour;
                    timeInfo.tm_min = minute;
                    timeInfo.tm_sec = second;
                    timeInfo.tm_isdst = -1;
                    
                    time_t epochTime = mktime(&timeInfo);
                    if (epochTime > 0) {
                        struct timeval tv = {epochTime, 0};
                        settimeofday(&tv, NULL);
                        timeObtained = true;
                        log_i("Time obtained from GSM: %d-%02d-%02d %02d:%02d:%02d", 
                              year, month, day, hour, minute, second);
                    }
                }
            }
        } else if (networkState.wifiConnected) {
            // Use WiFi NTP sync
            log_i("Syncing time via WiFi NTP...");
            configTime(0, 0, ntpServer.c_str());
            
            // Wait for time sync with timeout
            unsigned long syncStart = millis();
            while (!getLocalTime(&timeInfo) && (millis() - syncStart) < 10000) {
                delay(500);
            }
            
            if (getLocalTime(&timeInfo)) {
                timeObtained = true;
                log_i("Time obtained from WiFi NTP: %d-%02d-%02d %02d:%02d:%02d", 
                      timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday,
                      timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
            }
        }
        
        if (!timeObtained && retry < TIME_SYNC_MAX_RETRY - 1) {
            log_w("Time sync failed, retrying in 5 seconds...");
            delay(5000);
        }
    }
    
    if (timeObtained) {
        // Update system data
        strftime(sysData->Date, sizeof(sysData->Date), "%d/%m/%Y", &timeInfo);
        strftime(sysData->Time, sizeof(sysData->Time), "%T", &timeInfo);
        sysData->currentDataTime = String(sysData->Date) + " " + String(sysData->Time);
        
        log_i("Time synchronized successfully: %s", sysData->currentDataTime.c_str());
        updateDisplayStatus(devInfo, sysStatus, DISP_EVENT_DATETIME_OK);
        
        // Update state
        if (xSemaphoreTake(networkStateMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            networkState.timeSync = true;
            xSemaphoreGive(networkStateMutex);
        }
        
        sysStatus->datetime = true;
        sendNetworkEvent(NET_EVENT_TIME_SYNCED);
        return true;
    } else {
        log_e("Failed to synchronize time after all retries");
        updateDisplayStatus(devInfo, sysStatus, DISP_EVENT_DATETIME_ERR);
        sysStatus->datetime = false;
        return false;
    }
}

// Send data to server
static bool sendDataToServer(send_data_t *dataToSend, deviceNetworkInfo_t *devInfo, 
                           systemStatus_t *sysStatus, systemData_t *sysData)
{
    if (!sslClient) {
        log_e("SSL client not initialized");
        return false;
    }
    
    if (!isNetworkConnected()) {
        log_e("No network connection available");
        return false;
    }
    
    // Validate required parameters
    if (devInfo->deviceid.length() == 0 || sysData->server.length() == 0) {
        log_e("Missing required parameters: deviceid or server");
        return false;
    }
    
    log_i("Sending data to server: %s", sysData->server.c_str());
    log_i("Device ID: %s", devInfo->deviceid.c_str());
    
    // Set SSL verification time
    time_t epochTime = mktime(&dataToSend->sendTimeInfo);
    if (epochTime <= 0) {
        log_e("Invalid timestamp in data to send");
        return false;
    }
    
    sslClient->setVerificationTime((epochTime / 86400UL) + 719528UL, epochTime % 86400UL);
    
    // Build POST data string
    String postData = "X-MSP-ID=" + devInfo->deviceid;
    
    // Add sensor data based on availability - check for valid non-zero values
    if (dataToSend->temp > -50.0 && dataToSend->temp < 100.0) {  // Reasonable temperature range
        postData += "&temp=" + String(dataToSend->temp, 3);
        postData += "&hum=" + String(dataToSend->hum, 3);
        postData += "&pre=" + String(dataToSend->pre, 3);
        postData += "&voc=" + String(dataToSend->VOC, 3);
        log_d("Added BME680 data: T=%.3f, H=%.3f, P=%.3f, VOC=%.3f", 
              dataToSend->temp, dataToSend->hum, dataToSend->pre, dataToSend->VOC);
    }
    
    if (dataToSend->MICS_CO > 0) {
        postData += "&cox=" + String(dataToSend->MICS_CO, 3);
        postData += "&nox=" + String(dataToSend->MICS_NO2, 3);
        postData += "&nh3=" + String(dataToSend->MICS_NH3, 3);
        log_d("Added MICS6814 data: CO=%.3f, NO2=%.3f, NH3=%.3f", 
              dataToSend->MICS_CO, dataToSend->MICS_NO2, dataToSend->MICS_NH3);
    }
    
    if (dataToSend->PM1 > 0) {
        postData += "&pm1=" + String(dataToSend->PM1);
        postData += "&pm25=" + String(dataToSend->PM25);
        postData += "&pm10=" + String(dataToSend->PM10);
        log_d("Added PMS5003 data: PM1=%d, PM2.5=%d, PM10=%d", 
              dataToSend->PM1, dataToSend->PM25, dataToSend->PM10);
    }
    
    if (dataToSend->ozone > 0) {
        postData += "&o3=" + String(dataToSend->ozone, 3);
        log_d("Added O3 data: %.3f", dataToSend->ozone);
    }
    
    postData += "&msp=" + String(dataToSend->MSP);
    postData += "&recordedAt=" + String(epochTime);
    
    log_d("POST data length: %d bytes", postData.length());
    
    // Attempt server connection with retries
    for (int retry = 0; retry < MAX_CONNECTION_RETRIES; retry++) {
        log_i("Server connection attempt %d/%d", retry + 1, MAX_CONNECTION_RETRIES);
        
        // Check if we still have network connectivity
        if (!isNetworkConnected()) {
            log_e("Network connection lost during server communication");
            return false;
        }
        
        if (sslClient->connect(sysData->server.c_str(), 443)) {
            log_i("Connected to server successfully");
            
            // Build HTTP request
            String httpRequest = "POST /api/v1/records HTTP/1.1\r\n";
            httpRequest += "Host: " + sysData->server + "\r\n";
            httpRequest += "Authorization: Bearer " + sysData->api_secret_salt + ":" + devInfo->deviceid + "\r\n";
            httpRequest += "Connection: close\r\n";
            httpRequest += "User-Agent: MilanoSmartPark/0.2\r\n";
            httpRequest += "Content-Type: application/x-www-form-urlencoded\r\n";
            httpRequest += "Content-Length: " + String(postData.length()) + "\r\n";
            httpRequest += "\r\n";
            httpRequest += postData;
            
            log_d("HTTP request size: %d bytes", httpRequest.length());
            
            // Send request
            size_t written = sslClient->print(httpRequest);
            if (written != httpRequest.length()) {
                log_w("Incomplete request sent: %d/%d bytes", written, httpRequest.length());
            }
            sslClient->flush();
            
            // Read response with timeout
            String response = "";
            unsigned long responseStart = millis();
            bool responseComplete = false;
            
            while (sslClient->connected() && 
                   (millis() - responseStart) < SERVER_RESPONSE_TIMEOUT_MS && 
                   !responseComplete) {
                
                if (sslClient->available()) {
                    String line = sslClient->readStringUntil('\n');
                    response += line + "\n";
                    responseStart = millis();  // Reset timeout on data received
                    
                    // Check if we have the complete response
                    if (response.indexOf("\r\n\r\n") != -1 || response.indexOf("\n\n") != -1) {
                        responseComplete = true;
                    }
                } else {
                    delay(10);
                }
            }
            
            sslClient->stop();
            
            log_d("Server response (%d bytes): %s", response.length(), response.substring(0, 200).c_str());
            
            // Check response status
            if (response.startsWith("HTTP/1.1 201") || response.startsWith("HTTP/1.1 200")) {
                log_i("Data sent successfully to server");
                sysData->sent_ok = true;
                sendNetworkEvent(NET_EVENT_DATA_SENT);
                return true;
            } else if (response.startsWith("HTTP/1.1 4")) {
                log_e("Client error (4xx): %s", response.substring(0, 50).c_str());
                // Don't retry on client errors (400-499)
                break;
            } else if (response.startsWith("HTTP/1.1 5")) {
                log_w("Server error (5xx): %s", response.substring(0, 50).c_str());
                // Retry on server errors (500-599)
            } else if (response.length() == 0) {
                log_w("No response received from server");
            } else {
                log_w("Unexpected server response: %s", response.substring(0, 50).c_str());
            }
            
        } else {
            log_w("Failed to connect to server (attempt %d)", retry + 1);
        }
        
        if (retry < MAX_CONNECTION_RETRIES - 1) {
            log_i("Retrying in %d ms...", NETWORK_RETRY_DELAY_MS);
            delay(NETWORK_RETRY_DELAY_MS);
        }
    }
    
    log_e("Failed to send data after all retries");
    sysData->sent_ok = false;
    return false;
}

// Update display status helper
static void updateDisplayStatus(deviceNetworkInfo_t *devInfo, systemStatus_t *sysStatus, displayEvents_t event)
{
    if (!devInfo || !sysStatus) {
        log_e("Invalid parameters for display update");
        return;
    }
    
    displayData.currentEvent = event;
    
    vMspOs_takeDataAccessMutex();
    displayData.devInfo = *devInfo;
    displayData.sysStat = *sysStatus;
    vMspOs_giveDataAccessMutex();
    
    tTaskDisplay_sendEvent(&displayData);
}

// Main network task
static void networkTask(void *pvParameters)
{
    log_i("Network Task started on core %d", xPortGetCoreID());
    
    // Use global data structures if available, otherwise create local defaults
    deviceNetworkInfo_t devInfo;
    systemStatus_t sysStatus;
    systemData_t sysData;
    
    if (globalSysData && globalSysStatus && globalDevInfo) {
        // Copy from global structures (with mutex protection)
        vMspOs_takeDataAccessMutex();
        devInfo = *globalDevInfo;
        sysStatus = *globalSysStatus;
        sysData = *globalSysData;
        vMspOs_giveDataAccessMutex();
        
        log_i("Network task using global data structures");
        log_i("Server: %s, Server OK: %d", sysData.server.c_str(), sysStatus.server_ok);
    } else {
        // Fallback to local initialization
        log_w("Global data structures not available, using local defaults");
        devInfo = {};
        sysStatus = {};
        sysData = {};
        
        // Initialize default values
        devInfo.wifipow = WIFI_POWER_17dBm;
        sysData.ntp_server = NTP_SERVER_DEFAULT;
        sysData.timezone = TZ_DEFAULT;
        
        // Set compile-time defaults
#ifdef API_SERVER
        sysData.server = API_SERVER;
        sysStatus.server_ok = true;
#endif
#ifdef API_SECRET_SALT
        sysData.api_secret_salt = API_SECRET_SALT;
#endif
    }
    
    // Initialize network resources
    if (!initializeNetworkResources()) {
        log_e("Failed to initialize network resources, task exiting");
        
        // Mark task as not running
        if (xSemaphoreTake(networkStateMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            networkState.taskRunning = false;
            xSemaphoreGive(networkStateMutex);
        }
        
        vTaskDelete(NULL);
        return;
    }
    
    // Load initial configuration from SD card
    sensorData_t sensorData = {};
    deviceMeasurement_t measStat = {};
    loadNetworkConfiguration(&devInfo, &sysStatus, &sysData, &sensorData, &measStat);
    
    // Print MAC address for identification
    uint8_t baseMac[6];
    esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
    sprintf(devInfo.baseMacChr, "%02X:%02X:%02X:%02X:%02X:%02X", 
            baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
    log_i("WiFi MAC: %s", devInfo.baseMacChr);
    updateDisplayStatus(&devInfo, &sysStatus, DISP_EVENT_WIFI_MAC_ADDR);
    
    // Initialize state machine
    updateNetworkState(NETWRK_EVT_WAIT);
    
    log_i("Network task initialized, entering main loop");
    
    // Main task loop
    while (1) {
        // Update current state from next state - no mutex needed (internal state)
        networkState.currentState = networkState.nextState;
        
        netwkr_task_evt_t currentState = getNetworkState();
        
        switch (currentState) {
            case NETWRK_EVT_WAIT:
            {
                // Wait for events or periodic maintenance
                EventBits_t events = xEventGroupWaitBits(
                    networkEventGroup,
                    NET_EVT_DATA_READY | NET_EVT_TIME_SYNC_REQ | NET_EVT_CONNECT_REQ | NET_EVT_DISCONNECT_REQ | NET_EVT_CONFIG_UPDATED,
                    pdTRUE,  // Clear bits on exit
                    pdFALSE, // Wait for any bit
                    pdMS_TO_TICKS(30000)  // 30 second timeout for periodic checks
                );
                
                if (events & NET_EVT_CONFIG_UPDATED) {
                    log_i("Configuration update request received");
                    // Reload configuration from SD card
                    sensorData_t sensorData = {};
                    deviceMeasurement_t measStat = {};
                    loadNetworkConfiguration(&devInfo, &sysStatus, &sysData, &sensorData, &measStat);
                } else if (events & NET_EVT_CONNECT_REQ) {
                    log_i("Connection request received");
                    updateNetworkState(NETWRK_EVT_INIT_CONNECTION);
                } else if (events & NET_EVT_TIME_SYNC_REQ) {
                    log_i("Manual time sync request received (redundant - time syncs automatically after connection)");
                    // Time sync now happens automatically after connection, but we'll honor manual requests
                    updateNetworkState(NETWRK_EVT_SYNC_DATETIME);
                } else if (events & NET_EVT_DATA_READY) {
                    log_i("Data ready for transmission");
                    updateNetworkState(NETWRK_EVT_UPDATE_DATA);
                } else if (events & NET_EVT_DISCONNECT_REQ) {
                    log_i("Disconnect request received");
                    updateNetworkState(NETWRK_EVT_DEINIT_CONNECTION);
                } else {
                    // Timeout occurred - perform periodic maintenance
                    log_v("Network task periodic check");
                    
                    // Check connection health
                    bool wifiConnected = (WiFi.status() == WL_CONNECTED);
                    bool gsmConnected = (modem && modem->isGprsConnected());
                    
                    if (xSemaphoreTake(networkStateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        // Update connection status if changed
                        if (networkState.wifiConnected != wifiConnected) {
                            networkState.wifiConnected = wifiConnected;
                            log_i("WiFi connection status changed: %s", wifiConnected ? "connected" : "disconnected");
                        }
                        
                        if (networkState.gsmConnected != gsmConnected) {
                            networkState.gsmConnected = gsmConnected;
                            log_i("GSM connection status changed: %s", gsmConnected ? "connected" : "disconnected");
                        }
                        
                        xSemaphoreGive(networkStateMutex);
                    }
                }
                break;
            }
            
            case NETWRK_EVT_INIT_CONNECTION:
            {
                log_i("Initializing network connection...");
                bool connected = false;
                
                // Check retry limits
                int currentRetries = 0;
                if (xSemaphoreTake(networkStateMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                    currentRetries = networkState.connectionRetries;
                    xSemaphoreGive(networkStateMutex);
                }
                
                if (currentRetries >= MAX_CONNECTION_RETRIES) {
                    log_w("Maximum connection retries reached, backing off...");
                    delay(30000);  // 30 second backoff
                    
                    // Reset retry counter
                    if (xSemaphoreTake(networkStateMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                        networkState.connectionRetries = 0;
                        xSemaphoreGive(networkStateMutex);
                    }
                }
                
                if (!sysStatus.use_modem) {
                    // Try WiFi connection
                    log_i("Attempting WiFi connection...");
                    connected = handleWiFiConnection(&devInfo, &sysStatus);
                } else {
                    // Try GSM connection
                    log_i("Attempting GSM connection...");
                    connected = handleGSMConnection(&devInfo, &sysStatus);
                }
                
                if (connected) {
                    log_i("Network connection established successfully");
                    updateNetworkState(NETWRK_EVT_SYNC_DATETIME);
                } else {
                    log_w("Network connection failed, returning to wait state");
                    // Exponential backoff delay
                    int backoffDelay = NETWORK_RETRY_DELAY_MS * (1 << min(currentRetries, 4));
                    delay(backoffDelay);
                    updateNetworkState(NETWRK_EVT_WAIT);
                }
                break;
            }
            
            case NETWRK_EVT_SYNC_DATETIME:
            {
                log_i("Synchronizing date and time...");
                
                // Only sync if we have a connection
                if (!isNetworkConnected()) {
                    log_w("No network connection for time sync");
                    updateNetworkState(NETWRK_EVT_INIT_CONNECTION);
                    break;
                }
                
                if (syncDateTime(&devInfo, &sysStatus, &sysData)) {
                    log_i("Time synchronization successful");
                    
                    // Reset NTP sync counter on successful sync
                    if (xSemaphoreTake(networkStateMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                        networkState.ntpSyncExpired = NTP_SYNC_TX_COUNT;
                        xSemaphoreGive(networkStateMutex);
                    }
                } else {
                    log_w("Time synchronization failed, but continuing...");
                }
                
                updateNetworkState(NETWRK_EVT_WAIT);
                break;
            }
            
            case NETWRK_EVT_UPDATE_DATA:
            {
                log_i("*** NETWRK_EVT_UPDATE_DATA triggered - Processing data for transmission...");
                
                // Handle connection state based on NTP sync expiration
                bool needsConnection = false;
                bool needsTimeSync = false;
                
                // No mutex needed - internal state access within network task
                if (!networkState.wifiConnected && !networkState.gsmConnected) {
                    // Not connected, check if we need time sync
                    if (networkState.configurationLoaded) {
                        networkState.ntpSyncExpired--;
                        
                        if (networkState.ntpSyncExpired <= 0) {
                            log_i("NTP sync expired, full reconnection with time sync needed");
                            networkState.ntpSyncExpired = NTP_SYNC_TX_COUNT; // Reset counter
                            needsConnection = true;
                            needsTimeSync = true;
                        } else {
                            log_i("Connect without NTP sync (count: %d)", networkState.ntpSyncExpired);
                            needsConnection = true;
                            needsTimeSync = false;
                        }
                    }
                }
                
                // Handle connection requirements
                if (needsConnection) {
                    if (needsTimeSync) {
                        // Full connection with time sync
                        updateNetworkState(NETWRK_EVT_INIT_CONNECTION);
                        // Time sync will be handled automatically after connection
                        break;
                    } else {
                        // Connection without time sync
                        updateNetworkState(NETWRK_EVT_INIT_CONNECTION);
                        break;
                    }
                }
                
                // Check if we have a connection for data transmission
                if (!isNetworkConnected()) {
                    log_w("No network connection available for data transmission");
                    updateNetworkState(NETWRK_EVT_INIT_CONNECTION);
                    break;
                }
                
                // Process all queued data
                int processedCount = 0;
                int failedCount = 0;
                send_data_t currentData;
                
                log_i("Starting to process queued data...");
                while (dequeueSendData(&currentData, 0)) {  // Non-blocking dequeue
                    log_i("Processing queued data item %d", processedCount + 1);
                    
                    // Send data to server if connection and time sync are OK
                    // No mutex needed - internal state access within network task
                    bool canSendData = (networkState.wifiConnected || networkState.gsmConnected) && 
                                      networkState.timeSync && sysStatus.server_ok;
                    
                    // Debug logging to trace connection conditions
                    log_i("Connection check: WiFi=%d, GSM=%d, TimeSync=%d, ServerOK=%d", 
                          networkState.wifiConnected, networkState.gsmConnected, 
                          networkState.timeSync, sysStatus.server_ok);
                    
                    if (canSendData) {
                        // Show upload status on display
                        updateDisplayStatus(&devInfo, &sysStatus, DISP_EVENT_URL_UPLOAD_STAT);
                        
                        if (sendDataToServer(&currentData, &devInfo, &sysStatus, &sysData)) {
                            processedCount++;
                            log_i("Data item %d sent successfully to server", processedCount);
                            
                            // The sendDataToServer function already sends NET_EVENT_DATA_SENT
                            // and updates sysData->sent_ok = true when successful
                        } else {
                            failedCount++;
                            log_e("Failed to send data item to server, re-queuing for later retry");
                            
                            // Send network error event
                            sendNetworkEvent(NET_EVENT_ERROR);
                            updateDisplayStatus(&devInfo, &sysStatus, DISP_EVENT_NETWORK_ERROR);
                            
                            // Re-queue the data for later retry (with timeout to avoid blocking)
                            if (!enqueueSendData(currentData, pdMS_TO_TICKS(1000))) {
                                log_e("Failed to re-queue data, data lost!");
                            }
                            
                            // Stop processing on failure to avoid continuous failures
                            break;
                        }
                    } else {
                        log_w("Cannot send data - conditions not met: WiFi=%d, GSM=%d, TimeSync=%d, ServerOK=%d", 
                              networkState.wifiConnected, networkState.gsmConnected, 
                              networkState.timeSync, sysStatus.server_ok);
                    }
                    
                    // Always log to SD card if available
                    log_d("Writing log to SD card...");
                    if (sysStatus.sdCard) {
                        if (uHalSdcard_checkLogFile(&devInfo)) {
                            // Use a local sensor data structure - will be populated by the functions that need it
                            sensorData_t localSensorData;
                            memset(&localSensorData, 0, sizeof(sensorData_t));
                            
                            vHalSdcard_logToSD(&currentData, &sysData, &sysStatus, &localSensorData, &devInfo);
                            log_d("Data logged to SD card successfully");
                        } else {
                            log_w("SD card log file check failed");
                        }
                    } else {
                        log_w("SD card not available for logging");
                    }
                    
                    // Print measurements to serial
                    log_d("Printing measurements to serial...");
                    sensorData_t localSensorData;
                    memset(&localSensorData, 0, sizeof(sensorData_t));
                    vHalSensor_printMeasurementsOnSerial(&currentData, &localSensorData);
                    
                    // Handle modem disconnection for power saving
                    if (sysStatus.use_modem && (networkState.gsmConnected || modem)) {
                        if (vHalNetwork_modemDisconnect()) {
                            if (xSemaphoreTake(networkStateMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                                networkState.gsmConnected = false;
                                xSemaphoreGive(networkStateMutex);
                            }
                            log_i("Modem disconnected to save power");
                        }
                    }
                    
                    // Small delay between transmissions
                    delay(100);
                }
                
                if (processedCount > 0) {
                    log_i("Successfully processed %d data items", processedCount);
                }
                
                if (failedCount > 0) {
                    log_w("Failed to process %d data items", failedCount);
                }
                
                updateNetworkState(NETWRK_EVT_WAIT);
                break;
            }
            
            case NETWRK_EVT_DEINIT_CONNECTION:
            {
                log_i("Deinitializing network connections...");
                
                // Disconnect WiFi
                if (WiFi.status() == WL_CONNECTED) {
                    WiFi.disconnect();
                    WiFi.mode(WIFI_OFF);
                    log_i("WiFi disconnected and turned off");
                }
                
                // Disconnect GSM
                if (modem && modem->isGprsConnected()) {
                    if (modem->gprsDisconnect()) {
                        log_i("GPRS disconnected successfully");
                    } else {
                        log_w("GPRS disconnect failed");
                    }
                }
                
                // Update connection states
                if (xSemaphoreTake(networkStateMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                    networkState.wifiConnected = false;
                    networkState.gsmConnected = false;
                    networkState.timeSync = false;
                    networkState.connectionRetries = 0;
                    xSemaphoreGive(networkStateMutex);
                }
                
                sysStatus.connection = false;
                log_i("Network deinitialization completed");
                
                sendNetworkEvent(NET_EVENT_DISCONNECTED);
                updateNetworkState(NETWRK_EVT_WAIT);
                break;
            }
            
            case NETWRK_EVT_FW_UPDATE_CHECK:
            {
                log_i("Checking for firmware updates...");
                
                // TODO: Implement firmware update check logic
                // This would involve:
                // 1. Connecting to update server
                // 2. Checking current firmware version
                // 3. Downloading and verifying new firmware if available
                // 4. Initiating OTA update process
                
                log_i("Firmware update check completed (not implemented)");
                updateNetworkState(NETWRK_EVT_WAIT);
                break;
            }
            
            default:
            {
                log_w("Unknown network state: %d, returning to wait state", currentState);
                updateNetworkState(NETWRK_EVT_WAIT);
                break;
            }
        }
        
        // Small delay to prevent excessive CPU usage and allow other tasks to run
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    // This should never be reached, but cleanup just in case
    log_w("Network task exiting unexpectedly");
    cleanupNetworkResources();
    
    // Mark task as not running
    if (xSemaphoreTake(networkStateMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        networkState.taskRunning = false;
        xSemaphoreGive(networkStateMutex);
    }
    
    vTaskDelete(NULL);
}

// Legacy function wrappers for backward compatibility
void vHalNetwork_printWiFiMACAddr(systemStatus_t *p_tSys, deviceNetworkInfo_t *p_tDev)
{
    if (!p_tSys || !p_tDev) {
        log_e("Invalid parameters for MAC address function");
        return;
    }
    
    uint8_t baseMac[6];
    esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
    sprintf(p_tDev->baseMacChr, "%02X:%02X:%02X:%02X:%02X:%02X", 
            baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
    
    log_i("WiFi MAC Address: %s", p_tDev->baseMacChr);
    updateDisplayStatus(p_tDev, p_tSys, DISP_EVENT_WIFI_MAC_ADDR);
}

void vHalNetwork_connAndGetTime(systemStatus_t *p_tSys, deviceNetworkInfo_t *p_tDev, 
                                systemData_t *p_tSysData, tm *p_tm)
{
    if (!p_tSys || !p_tDev || !p_tSysData || !p_tm) {
        log_e("Invalid parameters for connection and time sync");
        return;
    }
    
    log_i("Legacy function: connect and get time");
    
    // Signal the network task to connect and sync time
    requestNetworkConnection();
    requestTimeSync();
    
    // Wait for connection with timeout
    if (waitForNetworkEvent(NET_EVENT_CONNECTED, pdMS_TO_TICKS(60000))) {
        p_tSys->connection = true;
        log_i("Connection established");
        
        // Wait for time sync with timeout
        if (waitForNetworkEvent(NET_EVENT_TIME_SYNCED, pdMS_TO_TICKS(30000))) {
            // Get current time
            if (getLocalTime(p_tm)) {
                p_tSys->datetime = true;
                log_i("Time synchronization completed successfully");
            } else {
                p_tSys->datetime = false;
                log_e("Failed to get local time after sync");
            }
        } else {
            p_tSys->datetime = false;
            log_e("Time synchronization timeout");
        }
    } else {
        p_tSys->connection = false;
        p_tSys->datetime = false;
        log_e("Connection timeout");
    }
}

void vHalNetwork_connectToInternet(systemStatus_t *p_tSys, deviceNetworkInfo_t *p_tDev)
{
    if (!p_tSys || !p_tDev) {
        log_e("Invalid parameters for internet connection");
        return;
    }
    
    log_i("Legacy function: connect to internet");
    
    // Signal the network task to establish connection
    requestNetworkConnection();
    
    // Wait for connection with timeout
    if (waitForNetworkEvent(NET_EVENT_CONNECTED, pdMS_TO_TICKS(45000))) {
        p_tSys->connection = true;
        log_i("Internet connection established successfully");
    } else {
        p_tSys->connection = false;
        log_e("Internet connection timeout or failed");
    }
}

void vHalNetwork_connectToServer(SSLClient *p_tClient, send_data_t *p_tDataToSent, 
                                deviceNetworkInfo_t *p_tDev, sensorData_t *p_tData, 
                                systemData_t *p_tSysData)
{
    // This legacy function is now handled by the network task
    // Enqueue the data for transmission
    if (p_tDataToSent) {
        log_i("Legacy function: enqueuing data for server transmission");
        
        if (enqueueSendData(*p_tDataToSent, pdMS_TO_TICKS(5000))) {
            log_i("Data enqueued successfully for transmission");
        } else {
            log_e("Failed to enqueue data for transmission");
        }
    } else {
        log_e("Invalid data pointer for server connection");
    }
}

// Load network configuration from SD card
static bool loadNetworkConfiguration(deviceNetworkInfo_t *devInfo, systemStatus_t *sysStatus, 
                                   systemData_t *sysData, sensorData_t *sensorData, 
                                   deviceMeasurement_t *measStat)
{
    log_i("Loading network configuration from SD card...");
    
    // Initialize SD card if not already done
    if (!initializeSD(sysStatus, devInfo)) {
        log_e("Failed to initialize SD card");
        return false;
    }
    
    // Load configuration from SD card
    bool configLoaded = checkConfig("/config_v3.txt", devInfo, sensorData, measStat, sysStatus, sysData);
    
    if (configLoaded) {
        log_i("Network configuration loaded successfully");
        log_i("WiFi SSID: %s", devInfo->ssid.c_str());
        log_i("GSM APN: %s", devInfo->apn.c_str());
        log_i("Device ID: %s", devInfo->deviceid.c_str());
        log_i("Server: %s", sysData->server.c_str());
        log_i("Server OK status: %d", sysStatus->server_ok);
        log_i("Use modem: %s", sysStatus->use_modem ? "yes" : "no");
        
        // If server_ok is not set, check if we should fall back to API_SERVER
        if (!sysStatus->server_ok || sysData->server.length() == 0) {
            log_i("Server not configured from SD card, checking for compile-time fallback...");
#ifdef API_SERVER
            log_i("Using compile-time API_SERVER: %s", API_SERVER);
            sysData->server = API_SERVER;
            sysStatus->server_ok = true;
            log_i("Server OK status updated to: %d", sysStatus->server_ok);
#else
            log_e("No server configured and no compile-time API_SERVER defined!");
            sysStatus->server_ok = false;
#endif
        }
        
        // Update state
        if (xSemaphoreTake(networkStateMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            networkState.configurationLoaded = true;
            xSemaphoreGive(networkStateMutex);
        }
        
        return true;
    } else {
        log_e("Failed to load network configuration");
        updateDisplayStatus(devInfo, sysStatus, DISP_EVENT_SD_CARD_CONFIG_ERROR);
        return false;
    }
}

// Additional utility functions
bool isNetworkTaskRunning()
{
    bool running = false;
    
    if (xSemaphoreTake(networkStateMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        running = networkState.taskRunning;
        xSemaphoreGive(networkStateMutex);
    }
    
    return running;
}

void requestNetworkDisconnection()
{
    log_i("Requesting network disconnection");
    if (networkEventGroup) {
        xEventGroupSetBits(networkEventGroup, NET_EVT_DISCONNECT_REQ);
    }
}

void requestNetworkConnection()
{
    log_i("Requesting network connection");
    if (networkEventGroup) {
        xEventGroupSetBits(networkEventGroup, NET_EVT_CONNECT_REQ);
    }
}

void requestTimeSync()
{
    log_i("Requesting time synchronization");
    if (networkEventGroup) {
        xEventGroupSetBits(networkEventGroup, NET_EVT_TIME_SYNC_REQ);
    }
}

void updateNetworkConfig()
{
    log_i("Requesting network configuration update");
    if (networkEventGroup) {
        xEventGroupSetBits(networkEventGroup, NET_EVT_CONFIG_UPDATED);
    }
}

bool getNetworkStatus(bool *wifiConnected, bool *gsmConnected, bool *timeSync)
{
    if (!wifiConnected || !gsmConnected || !timeSync) {
        return false;
    }
    
    if (xSemaphoreTake(networkStateMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        *wifiConnected = networkState.wifiConnected;
        *gsmConnected = networkState.gsmConnected;
        *timeSync = networkState.timeSync;
        xSemaphoreGive(networkStateMutex);
        return true;
    }
    
    return false;
}

// Network configuration initialization functions (moved from main file)
void vMspInit_setDefaultSslName(systemData_t *p_tData)
{
    // Default server name for SSL data upload
    p_tData->server = API_SERVER;
    p_tData->server_ok = true;
    log_i("Network SSL server set to: %s", p_tData->server.c_str());
}

void vMspInit_setApiSecSaltAndFwVer(systemData_t *p_tData)
{
    // API security salt
    p_tData->api_secret_salt = API_SECRET_SALT;
    log_i("API secret salt configured");
    
    // Current firmware version
    p_tData->ver = VERSION_STRING;
    log_i("Firmware version set to: %s", p_tData->ver.c_str());
}