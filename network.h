/*
                        Milano Smart Park Firmware
                       Developed by Norman Mulinacci

          This code is usable under the terms and conditions of the
             GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
*/

// Network Management Functions

#ifndef NETWORK_H
#define NETWORK_H


#include "shared_values.h"
#include "SSLClient.h"
#include <WiFi.h>

#define TIME_SYNC_MAX_RETRY 5




SSLClient* getGSMClient();

void initializeModem();

bool modemDisconnect();

void printWiFiMACAddr(systemStatus_t *p_tSys, deviceNetworkInfo_t *p_tDev);

void connAndGetTime(systemStatus_t *p_tSys,deviceNetworkInfo_t *p_tDev, systemData_t *p_tSysData, tm *p_tm);

void connectToServer(SSLClient *p_tClient, send_data_t *p_tDataToSent, deviceNetworkInfo_t *p_tDev, sensorData_t *p_tData, systemData_t *p_tSysData);

void connectToInternet(systemStatus_t *p_tSys,deviceNetworkInfo_t *p_tDev);

#endif