/**
 * @file display.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-07-08
 * 
 * @copyright Copyright (c) 2025
 * 
 */


 // -- includes --
#include "icons/icons.h"
#include "display/display.h"
#include "genericFunctions/generic_functions.h"

#include <Wire.h>
// OLED display library
#include <U8g2lib.h>

#define XBM_X_POS_MSPICON 0
#define XBM_Y_POS_MSPICON 0
#define XBM__MSPICON_W    64
#define XMB__MSPICON_H    64   

#define XBM_X_POS_SDICON  72
#define XBM_Y_POS_SDICO   0
#define XBM_SDICON_W      16
#define XMB_SDICON_H      16

#define XBM_X_POS_CLKICON  92
#define XBM_Y_POS_CLKICON  0
#define XBM_CLKICON_W      16
#define XMB_CLKICON_H      16

#define XBM_X_POS_MOBICON  112
#define XBM_Y_POS_MOBICON  0
#define XBM_MOBICON_W      16
#define XMB_MOBICON_H      16

#define XBM_X_POS_WIFIICON  112
#define XBM_Y_POS_WIFICON   0
#define XBM_WIFIICON_W      16
#define XMB_WIFIICON_H      16

#define XBM_X_POS_NOCONICON  112
#define XBM_Y_POS_NOCONICON   0
#define XBM_NOCONICON_W      16
#define XMB_NOCONICON_H      16

#define XBM_X_POS_LINE    0
#define XBM_Y_POS_LINE    17
#define XBM_LINE_W        127 
#define XMB_LINE_H        17

#define DRAW_STR_X_POS     74
#define DRAW_STR_Y_POS_FIRST_NAME  12
#define DRAW_STR_Y_POS_MID_NAME    25
#define DRAW_STR_Y_POS_LAST_NAME   38

#define SET_CRSR_X_POS_AUTHOR      37
#define SET_CRSR_Y_POS_AUTHOR      62
#define SET_CRSR_X_POS_FWVER       74
#define SET_CRSR_Y_POS_FWVER       62

#define POS_X_DEVICE_ID           0
#define POS_Y_DEVICE_ID           13

#define STR_FIRST_NAME        "Milano"
#define STR_SECOND_NAME       "Smart"
#define STR_LAST_NAME         "Park"
#define STR_AUTHOR            "by NM"


// I2C bus pins
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

// -- Instance for the OLED 1.3" display with the SH1106 controller
static U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, I2C_SCL_PIN, I2C_SDA_PIN); // ESP32 Thing, HW I2C with pin remapping


void initSerialAndI2cDisplay(void)
{
  // INIT SERIAL, I2C, DISPLAY ++++++++++++++++++++++++++++++++++++
  Serial.begin(115200);
  delay(2000); // give time to serial to initialize properly
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  u8g2.begin();
}

/**
 * @brief  draws the boot screen on the U8G2 display
 * 
 * @param fwver 
 */
 void drawBoot(String *fwver) 
 { 
  u8g2.firstPage();
  u8g2.clearBuffer();
  u8g2.drawXBM(XBM_X_POS_MSPICON,XBM_Y_POS_MSPICON,XBM__MSPICON_W,XMB__MSPICON_H,icons.msp_icon64x64);
  u8g2.setFont(u8g2_font_6x13B_tf);
  u8g2.drawStr(DRAW_STR_X_POS,DRAW_STR_Y_POS_FIRST_NAME,STR_FIRST_NAME); 
  u8g2.drawStr(DRAW_STR_X_POS,DRAW_STR_Y_POS_MID_NAME,STR_SECOND_NAME);
  u8g2.drawStr(DRAW_STR_X_POS,DRAW_STR_Y_POS_LAST_NAME,STR_LAST_NAME);
  u8g2.setFont(u8g2_font_6x13_mf);
  u8g2.setCursor(SET_CRSR_X_POS_AUTHOR,SET_CRSR_Y_POS_AUTHOR); u8g2.print(STR_AUTHOR);
  u8g2.setCursor(SET_CRSR_X_POS_FWVER,SET_CRSR_Y_POS_FWVER); u8g2.print(*fwver);
  u8g2.sendBuffer();
//   delay(5000);
}

/**********************************************************
 * @brief  draws the screen header on the U8G2 display
 * 
 **********************************************************/
void drawScrHead(systemStatus_t *statPtr, deviceNetworkInfo_t *devinfoPtr)
{ 
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x13_tf);

  // system state
  u8g2.setCursor(POS_X_DEVICE_ID,POS_Y_DEVICE_ID); u8g2.print("#" + devinfoPtr->deviceid + "#");

  if (statPtr->sdCard) 
  {
    u8g2.drawXBMP(XBM_X_POS_SDICON, XBM_Y_POS_SDICO, XBM_SDICON_W, XMB_SDICON_H, icons.sd_icon16x16);
  }
  if (statPtr->datetime) 
  {
    u8g2.drawXBMP(XBM_X_POS_CLKICON, XBM_Y_POS_CLKICON, XBM_CLKICON_W, XMB_CLKICON_H, icons.clock_icon16x16);
  }
  if (statPtr->connection) 
  {
    if (statPtr->use_modem) 
    {
      u8g2.drawXBMP(XBM_X_POS_MOBICON, XBM_Y_POS_MOBICON, XBM_MOBICON_W, XMB_MOBICON_H, icons.mobile_icon16x16);
    } 
    else 
    {
     u8g2.drawXBMP(XBM_X_POS_WIFIICON, XBM_Y_POS_WIFICON, XBM_WIFIICON_W, XMB_WIFIICON_H, icons.wifi1_icon16x16);
    }
  }
  else 
  {
    u8g2.drawXBMP(XBM_X_POS_NOCONICON, XBM_Y_POS_NOCONICON, XBM_NOCONICON_W, XMB_NOCONICON_H, icons.nocon_icon16x16);
  }

  u8g2.drawLine(XBM_X_POS_LINE, XBM_Y_POS_LINE, XBM_LINE_W, XMB_LINE_H);

}

/******************************************************************************
 * @brief get the proper horizontal offset of a string for the U8G2 display
 * 
 * @param string 
 * @return short 
 ******************************************************************************/
short getLineHOffset(const char string[]) 
{ 
  short offset = 0;
  u8g2_uint_t x = (u8g2.getDisplayWidth() - u8g2.getStrWidth(string)) / 2;
  if (x > 0) {
    offset = short(x);
  }
  return offset;
}


/***************************************************************************
 * @brief draws a text line on the U8G2 display
 * 
 * @param message 
 * @param secdelay 
 ***************************************************************************/
void drawLine(const char message[], short secdelay,systemStatus_t *statPtr, deviceNetworkInfo_t *devinfoPtr)
{
  short offset = getLineHOffset(message);
  drawScrHead(statPtr,devinfoPtr);
  u8g2.setCursor(offset, 45); u8g2.print(message);
  u8g2.sendBuffer();
  delay(secdelay * 1000);

}

/******************************************************
 * @brief draws two text lines on the U8G2 display
 * 
 * @param message1 
 * @param message2 
 * @param secdelay 
 ******************************************************/
void drawTwoLines(const char message1[], const char message2[], short secdelay,systemStatus_t *statPtr, deviceNetworkInfo_t *devinfoPtr) 
{
  short offset1 = getLineHOffset(message1);
  short offset2 = getLineHOffset(message2);

  drawScrHead(statPtr,devinfoPtr);
  u8g2.setCursor(offset1, 35); u8g2.print(message1);
  u8g2.setCursor(offset2, 55); u8g2.print(message2);
  u8g2.sendBuffer();
  delay(secdelay * 1000);
}

/***********************************************************
 * @brief draws a countdown on the U8G2 display
 * 
 * @param startsec 
 * @param message 
 ***********************************************************/
void drawCountdown(short startsec, const char message[],systemStatus_t *statPtr, deviceNetworkInfo_t *devinfoPtr)
{
  for (short i = startsec; i >= 0; i--) 
  {
    char output[17] = {0};

    sprintf(output, "WAIT %02d:%02d MIN.", i / 60, i % 60);

    drawTwoLines(message,output,1,statPtr,devinfoPtr);
  }
}

/**
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
 */
void drawMeasurements(sensorData_t *p_tData,systemStatus_t *statPtr, deviceNetworkInfo_t *devinfoPtr)
{

  log_i("Printing measurements on display...");

  // page 1
  drawScrHead(statPtr,devinfoPtr);
  u8g2.setCursor(5, 28); u8g2.print("Temp:  " + ((p_tData->status.BME680Sensor) ? (dspFloatToComma(p_tData->gasData.temperature) + "*C") : ""));
  u8g2.setCursor(5, 39); u8g2.print("Hum:  " + ((p_tData->status.BME680Sensor) ? (dspFloatToComma(p_tData->gasData.humidity) + "%" ) : ""));
  u8g2.setCursor(5, 50); u8g2.print("Pre:  " + ((p_tData->status.BME680Sensor) ? (dspFloatToComma(p_tData->gasData.pressure) + "hPa") : ""));
  u8g2.setCursor(5, 61); u8g2.print("VOC:  " + ((p_tData->status.BME680Sensor) ? (dspFloatToComma(p_tData->gasData.volatileOrganicCompounds) + "kOhm") : ""));
  u8g2.sendBuffer();
  delay(7000);

  // page 2
  drawScrHead(statPtr,devinfoPtr);
  u8g2.setCursor(5, 28); u8g2.print("PM1:  " + ((p_tData->status.PMS5003Sensor) ? (String(p_tData->airQualityData.particleMicron1) + "ug/m3") : ""));
  u8g2.setCursor(5, 39); u8g2.print("PM2,5:  " + ((p_tData->status.PMS5003Sensor) ? (String(p_tData->airQualityData.particleMicron25) + "ug/m3") : ""));
  u8g2.setCursor(5, 50); u8g2.print("PM10:  " + ((p_tData->status.PMS5003Sensor) ? (String(p_tData->airQualityData.particleMicron10) + "ug/m3") : ""));
  u8g2.sendBuffer();
  delay(5000);

  // page 3
  drawScrHead(statPtr,devinfoPtr);
  u8g2.setCursor(5, 28); u8g2.print("CO:  " + ((p_tData->status.MICS6814Sensor) ? (dspFloatToComma(p_tData->pollutionData.data.carbonMonoxide) + "ug/m3") : ""));
  u8g2.setCursor(5, 39); u8g2.print("NOx:  " + ((p_tData->status.MICS6814Sensor) ? (dspFloatToComma(p_tData->pollutionData.data.nitrogenDioxide) + "ug/m3") : ""));
  u8g2.setCursor(5, 50); u8g2.print("NH3:  " + ((p_tData->status.MICS6814Sensor) ? (dspFloatToComma(p_tData->pollutionData.data.ammonia) + "ug/m3") : ""));
  u8g2.sendBuffer();
  delay(5000);

  // page 4
  drawScrHead(statPtr,devinfoPtr);
  u8g2.setCursor(5, 39); u8g2.print("O3:  " + ((p_tData->status.O3Sensor) ? (dspFloatToComma(p_tData->ozoneData.ozone) + "ug/m3") : ""));
  u8g2.sendBuffer();
  delay(3000);
  
}


/**
 * @brief draw input values to screen
 * 
 * @param redval 
 * @param oxval 
 * @param nh3val 
 */
void drawMicsValues(uint16_t redval, uint16_t oxval, uint16_t nh3val,systemStatus_t *statPtr, deviceNetworkInfo_t *devinfoPtr)
{

  log_d("MICS6814 stored base resistance values:");
  log_d("RED: %d | OX: %d | NH3: %d\n", redval, oxval, nh3val);
  drawScrHead(statPtr,devinfoPtr);
  u8g2.setCursor(2, 28); u8g2.print("MICS6814 Res0 values:");
  u8g2.setCursor(30, 39); u8g2.print("RED: " + String(redval));
  u8g2.setCursor(30, 50); u8g2.print("OX: " + String(oxval));
  u8g2.setCursor(30, 61); u8g2.print("NH3: " + String(nh3val));
  u8g2.sendBuffer();
  delay(5000);

}