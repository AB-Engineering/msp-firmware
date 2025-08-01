/**************************************************************************
 * @file    network.h
 * @author  Refactored by AB-Engineering - https://ab-engineering.it
 * @brief   network management functions for the Milano Smart Park project
 * @details Milano Smart Park Firmware
            Developed by Norman Mulinacci
            This code is usable under the terms and conditions of the
            GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
 * @version 0.1
 * @date 2025-07-25
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

#define NTP_SERVER_DEFAULT "pool.ntp.org"
#define TZ_DEFAULT "GMT0"
/************************************************************
 * @brief get the GSM client instance
 * 
 * @return SSLClient* 
 ************************************************************/
SSLClient* tHalNetwork_getGSMClient();

/***********************************************************
 * @brief disconnect from the modem
 * 
 * @return uint8_t 
 **********************************************************/
uint8_t vHalNetwork_modemDisconnect();

/*************************************************************************************
 * @brief print the WiFi MAC address to the Serial monitor and update the display
 * 
 * @param p_tSys 
 * @param p_tDev 
 *************************************************************************************/
void vHalNetwork_printWiFiMACAddr(systemStatus_t *p_tSys, deviceNetworkInfo_t *p_tDev);

/*************************************************************************************
 * @brief conect to the internet and retrieve the current date and time
 * 
 * @param p_tSys 
 * @param p_tDev 
 * @param p_tSysData 
 * @param p_tm 
 *************************************************************************************/
void vHalNetwork_connAndGetTime(systemStatus_t *p_tSys,deviceNetworkInfo_t *p_tDev, systemData_t *p_tSysData, tm *p_tm);

/*************************************************************************************
 * @brief connect to the server and send data
 * 
 * @param p_tClient 
 * @param p_tDataToSent 
 * @param p_tDev 
 * @param p_tData 
 * @param p_tSysData 
 *************************************************************************************/
void vHalNetwork_connectToServer(SSLClient *p_tClient, send_data_t *p_tDataToSent, deviceNetworkInfo_t *p_tDev, sensorData_t *p_tData, systemData_t *p_tSysData);

/*************************************************************************************
 * @brief connect to the internet using WiFi or GPRS depending on the system status
 * 
 * @param p_tSys 
 * @param p_tDev 
 *************************************************************************************/
void vHalNetwork_connectToInternet(systemStatus_t *p_tSys,deviceNetworkInfo_t *p_tDev);

#endif