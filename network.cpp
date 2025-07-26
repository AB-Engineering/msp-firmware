/****************************************************
 * @file    network.cpp
 * @author  AB-Engineering - https://ab-engineering.it
 * @brief   Network management for the Milano Smart Park project
 * @details This file contains functions to manage network connections, including WiFi and GSM modem.
 * @version 0.1
 * @date    2025-07-16
 * 
 * @copyright Copyright (c) 2025
 * 
 ****************************************************/

 // Select modem type
#define TINY_GSM_MODEM_SIM800

#include <Arduino.h>
#include <TinyGsmClient.h>
#include "time.h"
#include "network.h"
#include "trust_anchor.h"
#include "display_task.h"
#include "mspOs.h"

// -- defines
#define TIME_SYNC_MAX_RETRY 5
#define MODEM_TX 13
#define MODEM_RX 15

// Modem stuff
#if !defined(TINY_GSM_RX_BUFFER) // Increase RX buffer to capture the entire response
#define TINY_GSM_RX_BUFFER 650
#endif
// #define TINY_GSM_DEBUG Serial // Define the serial console for debug prints, if needed

// Pin to get semi-random data from for SSL
// Pick a pin that's not connected or attached to a randomish voltage source
const int rand_pin = 35;

HardwareSerial gsmSerial(1);

// Modem instance
/*
#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(gsmSerial, Serial);
TinyGsm        modem(debugger);
#else*/
TinyGsm modem(gsmSerial);
// #endif
TinyGsmClient gsm_base(modem);

SSLClient gsmclient(gsm_base, TAs, (size_t)TAs_NUM, rand_pin, 1, SSLClient::SSL_ERROR);

// -- queue data to display task 
static displayData_t displayData;

static void vMsp_updateNetworkData(deviceNetworkInfo_t *p_tDev,systemStatus_t *p_tSys,displayEvents_t event)
{
  displayData.currentEvent = event;

  vMspOs_takeDataAccessMutex();

  displayData.devInfo = *p_tDev;
  displayData.sysStat = *p_tSys;

  vMspOs_giveDataAccessMutex();
}

// ----------------------------- local function prototypes -----------------------------

static void disconnectWiFi(systemStatus_t *p_tSys);

static uint8_t connectWiFi(deviceNetworkInfo_t *p_tDev,systemStatus_t *p_tSys);

static uint8_t connectModem(systemStatus_t *p_tSys, deviceNetworkInfo_t *p_tDev);

static void vHalNetwork_initializeModem(void);

// ----------------------------- local function definitions -----------------------------

/******************************************
 * @brief disconnect wifi 
 * 
 * @param p_tSys 
 ******************************************/
void disconnectWiFi(systemStatus_t *p_tSys)
{
  log_i("Turning off WiFi...\n");
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  p_tSys->connection = false;
  delay(1000); // Waiting a bit for Wifi mode set
}


/****************************************************************
 * @brief connect wifi 
 * 
 * @param p_tDev 
 * @param p_tSys 
 * @return uint8_t 
 ****************************************************************/
uint8_t connectWiFi(deviceNetworkInfo_t *p_tDev,systemStatus_t *p_tSys)
{ // sets WiFi mode and tx power (var wifipow), performs connection to WiFi network (vars ssid, passw)

  log_i("Setting WiFi STATION mode...");
  WiFi.mode(WIFI_STA);      // Set WiFi to station mode
  delay(1000);              // Wait for mode to set
  WiFi.setTxPower(p_tDev->wifipow); // Set WiFi transmission power
  log_i("WIFIPOW set to %d",p_tDev->wifipow);
  log_i("Legend: -4(-1dBm), 8(2dBm), 20(5dBm), 28(7dBm), 34(8.5dBm), 44(11dBm), 52(13dBm), 60(15dBm), 68(17dBm), 74(18.5dBm), 76(19dBm), 78(19.5dBm)\n");

  for (short retry = 0; retry < 4; retry++)
  { // Scan WiFi for selected network and connect
    log_i("Scanning WiFi networks...");
    short networks = WiFi.scanNetworks(); // WiFi.scanNetworks will return the number of networks found
    log_i("Scanning complete\n");

    if (networks > 0)
    { // Looking through found networks
      log_i("%d networks found\n", networks);
      uint8_t ssid_ok = FALSE; // For selected network if found
      for (short i = 0; i < networks; i++)
      { // Prints SSID and RSSI for each network found, checks against ssid
        log_v("%d: %s(%d) %s%c", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "OPEN" : "ENCRYPTED", i == networks - 1 ? '\n' : ' ');
        if (WiFi.SSID(i) == p_tDev->ssid)
        {
          ssid_ok = TRUE;
          break;
        }
      }

      if (ssid_ok)
      { // Begin connection
        log_i("%s found!\n",  p_tDev->ssid.c_str());
        p_tDev->foundNet = p_tDev->ssid + " OK!";
        log_i("Connecting to %s, please wait...", p_tDev->ssid.c_str());

        WiFi.begin(p_tDev->ssid.c_str(), p_tDev->passw.c_str());
        auto start = millis(); // starting time
        while (WiFi.status() != WL_CONNECTED)
        {
          auto timeout = millis() - start;
          if (timeout > 10000)
          {
            WiFi.disconnect();
            log_e("Can't connect to network!\n");
            vMsp_updateNetworkData(p_tDev,p_tSys,DISP_EVENT_WIFI_DISCONNECTED);
            tTaskDisplay_sendEvent(&displayData);
            break;
          }
        }

        if (WiFi.status() == WL_CONNECTED)
        { // Connection successful
          log_i("Connection to WiFi made successfully!");
          return TRUE;
        }
      }
      else
      {
        log_e("%s not found!\n", p_tDev->ssid.c_str());
        p_tDev->noNet = "NO " + p_tDev->ssid + "!";
        vMsp_updateNetworkData(p_tDev,p_tSys,DISP_EVENT_SSID_NOT_FOUND);
        tTaskDisplay_sendEvent(&displayData);
      }
    }
    else
    {
      log_e("No networks found!\n");
      vMsp_updateNetworkData(p_tDev,p_tSys,DISP_EVENT_NO_NETWORKS_FOUND);
      tTaskDisplay_sendEvent(&displayData);
    }

    if (retry < 3)
    { // Print remaining tries
      log_i("Retrying, %d retries left\n", 3 - retry);
      p_tDev->remain = String(3 - retry) + " tries remain.";
      vMsp_updateNetworkData(p_tDev,p_tSys,DISP_EVENT_CONN_RETRY);
      tTaskDisplay_sendEvent(&displayData);
    }
    else if (retry == 3)
    {
      log_e("No internet connection!\n");
      vMsp_updateNetworkData(p_tDev,p_tSys,DISP_EVENT_NO_INTERNET);
      tTaskDisplay_sendEvent(&displayData);
    }
  }

  return FALSE;
}

/***********************************************************************
 * @brief connect modem 
 * 
 * @param p_tSys 
 * @param p_tDev 
 * @return uint8_t 
 ***********************************************************************/
uint8_t connectModem(systemStatus_t *p_tSys, deviceNetworkInfo_t *p_tDev)
{

  vHalNetwork_initializeModem();

  String CCID = modem.getSimCCID();
  String IMSI = modem.getIMSI();

  log_d("CCID: %s", CCID.c_str());

  log_d("IMSI: %s", IMSI.c_str());

  if (CCID.startsWith("ERROR", 0) || IMSI.startsWith("ERROR", 0))
  {
    log_e("No SIM found or SIM not inserted!");
    vMsp_updateNetworkData(p_tDev,p_tSys,DISP_EVENT_SIM_ERROR);
    tTaskDisplay_sendEvent(&displayData);
    return FALSE;
  }

  short retries = 0;
  while (retries < 2)
  {
    log_i("Waiting for network...");
    if (modem.waitForNetwork())
      break;
    log_e("Couldn't find network!");
    if (retries < 1)
    {
      log_i("Retrying in 10 seconds...");
      delay(10000);
    }
    retries++;
  }

  if (!modem.isNetworkConnected())
  {
    log_e("Network is not connected!");
    vMsp_updateNetworkData(p_tDev,p_tSys,DISP_EVENT_NETWORK_ERROR);
    tTaskDisplay_sendEvent(&displayData);
    return FALSE;
  }

  log_i("Mobile network found!");

  retries = 0;
  while (retries < 4)
  {
    log_i("Connecting to GPRS...");
    if (modem.gprsConnect(p_tDev->apn.c_str(), "", ""))
      break;
    log_e("Connection failed!");
    if (retries < 3)
    {
      log_i("Retrying in 5 seconds...");
      delay(5000);
    }
    retries++;
  }

  if (!modem.isGprsConnected())
  {
    log_e("GPRS is not connected!");
    vMsp_updateNetworkData(p_tDev,p_tSys,DISP_EVENT_GPRS_ERROR);
    tTaskDisplay_sendEvent(&displayData);
    return FALSE;
  }

  log_i("Connection to GPRS made successfully!");

  log_d("Operator: %s", modem.getOperator().c_str());

  log_d("Signal quality: %d", modem.getSignalQuality());

  return TRUE;
}

/*****************************************
 * @brief initialize modem 
 * 
 *****************************************/
static void vHalNetwork_initializeModem()
{

  log_i("Initializing SIM800L modem serial connection...");

  // Set GSM module baud rate
  gsmSerial.begin(9600, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(6000);

  // Restart takes quite some time
  log_i("Issuing modem reset (takes some time)...");
  modem.restart();

  log_i("Done!");

  log_d("Modem Name: %s", modem.getModemName().c_str());

  log_d("Modem Info: %s", modem.getModemInfo().c_str());

  log_d("IMEI: %s", modem.getIMEI().c_str());
}


//---------------------------- higher level functions -----------------------------

/***********************************************************
 * @brief get the address of the structure
 * 
 * @return SSLClient* 
 ***********************************************************/
SSLClient* tHalNetwork_getGSMClient() 
{
    return &gsmclient;
}

/*************************************************************************************
 * @brief print the WiFi MAC address to the Serial monitor and update the display
 * 
 * @param p_tSys 
 * @param p_tDev 
 *************************************************************************************/
void vHalNetwork_printWiFiMACAddr(systemStatus_t *p_tSys, deviceNetworkInfo_t *p_tDev)
{

  uint8_t baseMac[6];
  esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
  
  sprintf(p_tDev->baseMacChr, "%02X:%02X:%02X:%02X:%02X:%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
  Serial.println("WIFI MAC ADDRESS: " + String(p_tDev->baseMacChr) + "\n");

  vMsp_updateNetworkData(p_tDev,p_tSys,DISP_EVENT_WIFI_MAC_ADDR);
  tTaskDisplay_sendEvent(&displayData);

}

/***********************************************************
 * @brief disconnect from the modem
 * 
 * @return uint8_t 
 **********************************************************/
uint8_t vHalNetwork_modemDisconnect()
{
  log_i("Disconnecting from GPRS...");
  uint8_t retStatus = TRUE;

  if(modem.isGprsConnected())
  {
    retStatus = modem.gprsDisconnect();
  }

  return retStatus;
}

/*************************************************************************
 * @brief connect and get time 
 * 
 * @param p_tSys 
 * @param p_tDev 
 * @param p_tSysData 
 * @param p_tm 
 *************************************************************************/
void vHalNetwork_connAndGetTime(systemStatus_t *p_tSys,deviceNetworkInfo_t *p_tDev, systemData_t *p_tSysData, tm *p_tm)
{ // connects to the internet and retrieves time from NTP server

  p_tSys->datetime = false; // resetting the date&time var
  if (p_tSys->use_modem == false)
  {
    configTime(0, 0, p_tSysData->ntp_server.c_str()); // configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2)
    Serial.println("Connecting to WiFi...\n");
    vMsp_updateNetworkData(p_tDev,p_tSys,DISP_EVENT_CONN_TO_WIFI);
    tTaskDisplay_sendEvent(&displayData);
    p_tSys->connection = connectWiFi(p_tDev,p_tSys);
  }
  else if (!modem.isNetworkConnected() || !modem.isGprsConnected())
  { // reconnect only if network is lost
    Serial.println("Connecting to GPRS...\n");
    vMsp_updateNetworkData(p_tDev,p_tSys,DISP_EVENT_CONN_TO_GPRS);
    tTaskDisplay_sendEvent(&displayData);
    p_tSys->connection = connectModem(p_tSys,p_tDev);
  }
  if (p_tSys->connection)
  {
    Serial.println("Retrieving date&time...");
    vMsp_updateNetworkData(p_tDev,p_tSys,DISP_EVENT_RETREIVE_DATETIME);
    tTaskDisplay_sendEvent(&displayData);
    String tzRule = p_tSysData->timezone; // timezone rule
    if (tzRule.length() == 0)
    {
      log_e("TIMEZONE value is empty! Use default value: CET-1CEST");
      tzRule = "CET-1CEST"; // Default timezone rule
    }
    else
    {
      log_i("TIMEZONE = *%s*", tzRule.c_str());
    }
    setenv("TZ", tzRule.c_str(), 1);              // Setting timezone
    tzset();                                      // Apply timezone settings
    int8_t timeSyncRetries = TIME_SYNC_MAX_RETRY; // Number of retries for time synchronization

    auto start = millis();
    while ((p_tSys->datetime == 0) && (timeSyncRetries >= 0))
    { // Retrieving date&time
      timeSyncRetries--;
      auto timeout = millis() - start;
      if (p_tSys->use_modem == true)
      {
        int hh = 0, mm = 0, ss = 0, yyyy = 0, mon = 0, day = 0;
        float tz = 0;
        modem.NTPServerSync(p_tSysData->ntp_server, 0);
        p_tSys->datetime = modem.getNetworkTime(&yyyy, &mon, &day, &hh, &mm, &ss, &tz);
        p_tm->tm_hour = hh;
        p_tm->tm_min = mm;
        p_tm->tm_sec = ss;
        p_tm->tm_year = yyyy - 1900;
        p_tm->tm_mon = mon - 1;
        p_tm->tm_mday = day;
        p_tm->tm_isdst = -1;
        time_t t = mktime(p_tm);
        timeval now = { .tv_sec = t };
        settimeofday(&now, NULL);
      }
      else
      {
        p_tSys->datetime = getLocalTime(p_tm);
      }
      if (timeout > 90000) break;
    }

    if (p_tSys->datetime)
    {
      vMsp_updateNetworkData(p_tDev,p_tSys,DISP_EVENT_DATETIME_OK);
      tTaskDisplay_sendEvent(&displayData);
      strftime(p_tSysData->Date, sizeof(p_tSysData->Date), "%d/%m/%Y", p_tm); // Formatting date as DD/MM/YYYY
      strftime(p_tSysData->Time, sizeof(p_tSysData->Time), "%T", p_tm);       // Formatting time as HH:MM:SS
      p_tSysData->currentDataTime = String(p_tSysData->Date) + " " + String(p_tSysData->Time);
      Serial.println("Current date&time: " + p_tSysData->currentDataTime);
      vMsp_updateNetworkData(p_tDev,p_tSys,DISP_EVENT_DATETIME);
      tTaskDisplay_sendEvent(&displayData);
      
    }
    else
    {
      log_e("Failed to obtain date&time! Are you connected to the internet?\n");
      vMsp_updateNetworkData(p_tDev,p_tSys,DISP_EVENT_DATETIME_ERR);
      tTaskDisplay_sendEvent(&displayData);
    }
    Serial.println();
    delay(3000);
  }
}

/***********************************************************************
 * @brief connect to internet 
 * 
 * @param p_tSys 
 * @param p_tDev 
 ***********************************************************************/
void vHalNetwork_connectToInternet(systemStatus_t *p_tSys, deviceNetworkInfo_t *p_tDev)
{
  if (p_tSys->use_modem == false)
  {
    Serial.println("Connecting to WiFi...\n");
    vMsp_updateNetworkData(p_tDev,p_tSys,DISP_EVENT_CONN_TO_WIFI);
    tTaskDisplay_sendEvent(&displayData);
    p_tSys->connection = connectWiFi(p_tDev,p_tSys);
  }
  else if ((modem.isNetworkConnected() == false) || (modem.isGprsConnected() == false))
  { // reconnect only if network is lost
    Serial.println("Connecting to GPRS...\n");
    vMsp_updateNetworkData(p_tDev,p_tSys,DISP_EVENT_CONN_TO_GPRS);
    tTaskDisplay_sendEvent(&displayData);
    p_tSys->connection = connectModem(p_tSys,p_tDev);
  }
}

/********************************************************
 * @brief connect to server
 * 
 * @param p_tClient 
 * @param p_tDataToSent 
 * @param p_tDev 
 * @param p_tData 
 * @param p_tSysData 
 ********************************************************/
void vHalNetwork_connectToServer(SSLClient *p_tClient, send_data_t *p_tDataToSent, deviceNetworkInfo_t *p_tDev, sensorData_t *p_tData, systemData_t *p_tSysData)
{

  time_t epochTime = mktime(&p_tDataToSent->sendTimeInfo); // converting UTC date&time in UNIX Epoch Time format

  p_tClient->setVerificationTime((epochTime / 86400UL) + 719528UL, epochTime % 86400UL); // setting SLLClient's verification time to current time while converting UNIX Epoch Time to BearSSL's expected format

  auto start = millis(); // time for connection
  Serial.println("Uploading data to server through HTTPS in progress...\n");
  // vHalDisplay_drawTwoLines("Uploading data", "to server...", 0);

  log_i("Connecting to %s...\n", p_tSysData->server.c_str());
  log_i("Device ID: %s\n", p_tDev->deviceid.c_str());
  log_i("api_secret_salt: %s\n", p_tSysData->api_secret_salt.c_str());

  short retries = 0;
  while (retries < 4)
  {
    if (p_tClient->connect(p_tSysData->server.c_str(), 443))
    {
      log_i("Connection to server made! Time: %d\n", millis() - start);
      // Building the post string:
      String postStr = "X-MSP-ID=" + p_tDev->deviceid;
      if (p_tData->status.BME680Sensor)
      {
        postStr += "&temp=";
        postStr += String(p_tDataToSent->temp, 3);
        postStr += "&hum=";
        postStr += String(p_tDataToSent->hum, 3);
        postStr += "&pre=";
        postStr += String(p_tDataToSent->pre, 3);
        postStr += "&voc=";
        postStr += String(p_tDataToSent->VOC, 3);
      }
      if (p_tData->status.MICS6814Sensor)
      {
        postStr += "&cox=";
        postStr += String(p_tDataToSent->MICS_CO, 3);
        postStr += "&nox=";
        postStr += String(p_tDataToSent->MICS_NO2, 3);
        postStr += "&nh3=";
        postStr += String(p_tDataToSent->MICS_NH3, 3);
      }
      if (p_tData->status.PMS5003Sensor)
      {
        postStr += "&pm1=";
        postStr += String(p_tDataToSent->PM1);
        postStr += "&pm25=";
        postStr += String(p_tDataToSent->PM25);
        postStr += "&pm10=";
        postStr += String(p_tDataToSent->PM10);
      }
      if (p_tData->status.O3Sensor)
      {
        postStr += "&o3=";
        postStr += String(p_tDataToSent->ozone, 3);
      }
      postStr += "&msp=";
      postStr += String(p_tDataToSent->MSP);
      postStr += "&recordedAt=";
      postStr += String(epochTime);

      // Sending client requests
      String postLine = "POST /api/v1/records HTTP/1.1\nHost: " + p_tSysData->server + "\nAuthorization: Bearer " + p_tSysData->api_secret_salt + ":" +p_tDev->deviceid + "\n";
      postLine += "Connection: close\nUser-Agent: MilanoSmartPark\nContent-Type: application/x-www-form-urlencoded\nContent-Length: " + String(postStr.length());
      log_d("Post line:\n%s\n", postLine.c_str());
      log_d("Post string: %s\n", postStr.c_str());
      p_tClient->print(postLine + "\n\n" + postStr);
      p_tClient->flush();

      // Get answer from server
      start = millis();
      String answLine = "";
      while (p_tClient->available())
      {
        char c = p_tClient->read();
        answLine += c;
        auto timeout = millis() - start;
        if (timeout > 10000)
          break;
      }
      p_tClient->stop(); // Stopping the client

      // Check server answer
      if (answLine.startsWith("HTTP/1.1 201 Created", 0))
      {
        log_i("Server answer ok! Data uploaded successfully!\n");
        // vHalDisplay_drawTwoLines("Data uploaded", "successfully!", 2);
        p_tSysData->sent_ok = true;
      }
      else
      {
        log_e("Server answered with an error! Data not uploaded!\n");
        log_e("The full answer is:\n%s\n", answLine.c_str());
        // vHalDisplay_drawTwoLines("Serv answ error!", "Data not sent!", 10);
      }
      break; // exit
    }
    else
    {
      log_e("Error while connecting to server!");
      String mesg = "";
      if (retries == 3)
      {
        log_e("Data not uploaded!\n");
        mesg = "Data not sent!";
      }
      else
      {
        log_i("Trying again, %d retries left...\n", 3 - retries);
        mesg = String(3 - retries) + " retries left...";
      }
      // vHalDisplay_drawTwoLines("Serv conn error!", mesg.c_str(), 10);
      retries++;
    }
  }
}
