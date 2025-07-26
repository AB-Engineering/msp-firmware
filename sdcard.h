/******************************************************************************************************
 * @file    sdcard.h
 * @author  Refactored by AB-Engineering - https://ab-engineering.it
 * @brief   SD Card and File Management Functions for the Milano Smart Park project
 * @details Milano Smart Park Firmware
            Developed by Norman Mulinacci
            This code is usable under the terms and conditions of the
            GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
 * @version 0.1
 * @date 2025-07-25
 * 
 * @copyright Copyright (c) 2025
 * 
 *****************************************************************************************************/

#ifndef SDCARD_H
#define SDCARD_H

// -- includes --
#include "shared_values.h"

/*********************************************************
 * @brief check log file 
 * 
 * @param p_tDev 
 * @return uint8_t 
 *********************************************************/
uint8_t uHalSdcard_checkLogFile(deviceNetworkInfo_t *p_tDev);

/*******************************************************************************
 * @brief log to SD card 
 * 
 * @param data 
 * @param p_tSysData 
 * @param p_tSys 
 * @param p_tData 
 * @param p_tDev 
 ******************************************************************************/
void vHalSdcard_logToSD(send_data_t *data, systemData_t *p_tSysData, systemStatus_t *p_tSys, sensorData_t *p_tData, deviceNetworkInfo_t *p_tDev);

/******************************************************
 * @brief read SD card 
 * 
 * @param p_tSys 
 * @param p_tDev 
 * @param p_tData 
 * @param pDev 
 * @param p_tSysData 
 *****************************************************/
void vHalSdcard_readSD(systemStatus_t *p_tSys,deviceNetworkInfo_t *p_tDev,sensorData_t *p_tData,deviceMeasurement_t *pDev,systemData_t *p_tSysData);


#endif