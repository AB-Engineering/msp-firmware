/*
                        Milano Smart Park Firmware
                       Developed by Norman Mulinacci

          This code is usable under the terms and conditions of the
             GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
*/

// Display Management Functions

#ifndef DISPLAY_H
#define DISPLAY_H

#include "shared_values.h"

void initSerialAndI2cDisplay(void);
/******************************************************
 * @brief  draws the boot screen on the U8G2 display
 * 
 * @param fwver 
 *****************************************************/
  void drawBoot(String *fwver);


/******************************************************
 * @brief  draws the screen header on the U8G2 display
 * 
 ******************************************************/
void drawScrHead(systemStatus_t *statPtr, deviceNetworkInfo_t *devinfoPtr);

/******************************************************
 * @brief get the proper horizontal offset of a string 
 *        for the U8G2 display
 * 
 * @param string 
 * @return short 
 ******************************************************/
short getLineHOffset(const char string[]);


/*****************************************************
 * @brief draws a text line on the U8G2 display
 * 
 * @param message 
 * @param secdelay 
 *****************************************************/
void drawLine(const char message[], short secdelay,systemStatus_t *statPtr, deviceNetworkInfo_t *devinfoPtr);


/*****************************************************
 * @brief draws two text lines on the U8G2 display
 * 
 * @param message1 
 * @param message2 
 * @param secdelay 
 *****************************************************/
void drawTwoLines(const char message1[], const char message2[], short secdelay, systemStatus_t *statPtr, deviceNetworkInfo_t *devinfoPtr);

/*****************************************************
 * @brief draws a countdown on the U8G2 display
 * 
 * @param startsec 
 * @param message 
 *****************************************************/
void drawCountdown(short startsec, const char message[],systemStatus_t *statPtr, deviceNetworkInfo_t *devinfoPtr);

/**
 * @brief draw input values to screen
 * 
 * @param redval 
 * @param oxval 
 * @param nh3val 
 */
void drawMicsValues(uint16_t redval, uint16_t oxval, uint16_t nh3val,systemStatus_t *statPtr, deviceNetworkInfo_t *devinfoPtr);


void vMsp_drawBme680GasSensorData(sensorData_t *p_tData,systemStatus_t *statPtr, deviceNetworkInfo_t *devinfoPtr);
void vMsp_drawPMS5003AirQualitySensorData(sensorData_t *p_tData,systemStatus_t *statPtr, deviceNetworkInfo_t *devinfoPtr);
void vMsp_drawMICS6814PollutionSensorData(sensorData_t *p_tData,systemStatus_t *statPtr, deviceNetworkInfo_t *devinfoPtr);
void vMsp_drawOzoneSensorData(sensorData_t *p_tData,systemStatus_t *statPtr, deviceNetworkInfo_t *devinfoPtr);

#endif