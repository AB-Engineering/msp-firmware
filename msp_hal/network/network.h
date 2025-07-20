/*
                        Milano Smart Park Firmware
                       Developed by Norman Mulinacci

          This code is usable under the terms and conditions of the
             GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
*/

// Network Management Functions

#ifndef NETWORK_H
#define NETWORK_H


#include "msp_hal/data.h"
#include "SSLClient.h"
#include <WiFi.h>

#define TIME_SYNC_MAX_RETRY 5


typedef struct __SEND_DATA__
{
  struct tm sendTimeInfo; /*!< Date and time of the data to be sent */
  float temp;
  float hum;
  float pre;
  float VOC;
  int32_t PM1;
  int32_t PM25;
  int32_t PM10;
  float MICS_CO;
  float MICS_NO2;
  float MICS_NH3;
  float ozone;
  int8_t MSP; /*!< MSP# Index */
} send_data_t;

SSLClient* getGSMClient();

void initializeModem();

bool modemDisconnect();

void printWiFiMACAddr(systemStatus_t *p_tSys, deviceNetworkInfo_t *p_tDev);

void connAndGetTime(systemStatus_t *p_tSys,deviceNetworkInfo_t *p_tDev, systemData_t *p_tSysData, tm *p_tm);

void connectToServer(SSLClient *p_tClient, send_data_t *p_tDataToSent, deviceNetworkInfo_t *p_tDev, sensorData_t *p_tData, systemData_t *p_tSysData);

void connectToInternet(systemStatus_t *p_tSys,deviceNetworkInfo_t *p_tDev);

#endif