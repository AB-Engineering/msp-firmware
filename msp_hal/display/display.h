/*
                        Milano Smart Park Firmware
                       Developed by Norman Mulinacci

          This code is usable under the terms and conditions of the
             GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
*/

// Display Management Functions

#ifndef DISPLAY_H
#define DISPLAY_H

#include "msp_hal/data.h"

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

/*****************************************************
 * @brief  draws measurements on the U8g2 display
 * 
 * @param _temp 
 * @param _hum 
 * @param _pre 
 * @param _VOC 
 * @param _PM1 
 * @param _PM25 
 * @param _PM10 
 * @param _MICS_CO 
 * @param _MICS_NO2 
 * @param _MICS_NH3 
 * @param _ozone 
 *****************************************************/
void drawMicsValues(uint16_t redval, uint16_t oxval, uint16_t nh3val,systemStatus_t *statPtr, deviceNetworkInfo_t *devinfoPtr);

void drawMeasurements(sensorData_t *p_tData,systemStatus_t *statPtr, deviceNetworkInfo_t *devinfoPtr);
#endif