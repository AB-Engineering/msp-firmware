/*
                        Milano Smart Park Firmware
                       Developed by Norman Mulinacci

          This code is usable under the terms and conditions of the
             GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
*/

// SD Card and File Management Functions

#ifndef SDCARD_H
#define SDCARD_H

#include "msp_hal/display/display.h"
#include "msp_hal/network/network.h"
#include "msp_hal/data.h"


uint8_t initializeSD(systemStatus_t *p_tSys, deviceNetworkInfo_t *p_tDev);

uint8_t parseConfig(File fl, deviceNetworkInfo_t *p_tDev, sensorData_t *p_tData, deviceMeasurement_t *pDev, systemStatus_t *sysStat, systemData_t *p_tSysData);

uint8_t checkConfig(const char *configpath, deviceNetworkInfo_t *p_tDev, sensorData_t *p_tData, deviceMeasurement_t *pDev, systemStatus_t *p_tSys, systemData_t *p_tSysData);

uint8_t checkLogFile(deviceNetworkInfo_t *p_tDev);

uint8_t addToLog(const char *path, const char *oldpath, const char *errpath, String *message);

void logToSD(send_data_t *data, systemData_t *p_tSysData, systemStatus_t *p_tSys, sensorData_t *p_tData, deviceNetworkInfo_t *p_tDev);

void readSD(systemStatus_t *p_tSys,deviceNetworkInfo_t *p_tDev,sensorData_t *p_tData,deviceMeasurement_t *pDev,systemData_t *p_tSysData);


//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#endif