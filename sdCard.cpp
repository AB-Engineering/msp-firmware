/************************************************************************************************
 * @file    sdCard.cpp
 * @author  AB-Engineering - https://ab-engineering.it
 * @brief   Management of the SD Card functionalities for the Milano Smart Park project
 * @version 0.1
 * @date    2025-07-25
 *
 * @copyright Copyright (c) 2025
 *
 ************************************************************************************************/

// -- includes --
#include <U8g2lib.h>
#include <WiFi.h>
#include <SD.h>
#include <ArduinoJson.h>

#include "sdcard.h"
#include "generic_functions.h"
#include "display_task.h"
#include "display.h"
#include "network.h"
#include "mspOs.h"
#include "config.h"

//-------------------------- functions --------------------

static uint8_t parseConfig(File fl, deviceNetworkInfo_t *p_tDev, sensorData_t *p_tData, deviceMeasurement_t *pDev, systemStatus_t *sysStat, systemData_t *p_tSysData);
uint8_t initializeSD(systemStatus_t *p_tSys, deviceNetworkInfo_t *p_tDev);
uint8_t checkConfig(const char *configpath, deviceNetworkInfo_t *p_tDev, sensorData_t *p_tData, deviceMeasurement_t *pDev, systemStatus_t *p_tSys, systemData_t *p_tSysData);
uint8_t addToLog(const char *path, const char *oldpath, const char *errpath, String *message);

/**********************************************************************
 * @brief Send network data to display task queue using local structure
 *
 * @param p_tDev
 * @param p_tSys
 * @param event
 **********************************************************************/
static void vMsp_sendNetworkDataToDisplay(deviceNetworkInfo_t *p_tDev, systemStatus_t *p_tSys, displayEvents_t event)
{
  displayData_t localDisplayData = {};
  localDisplayData.currentEvent = event;

  vMspOs_takeDataAccessMutex();
  localDisplayData.devInfo = *p_tDev;
  localDisplayData.sysStat = *p_tSys;
  vMspOs_giveDataAccessMutex();

  tTaskDisplay_sendEvent(&localDisplayData);
}

/**********************************************************************
 * @brief initialize SD card
 *
 * @param p_tSys
 * @param p_tDev
 * @return uint8_t
 **********************************************************************/
uint8_t initializeSD(systemStatus_t *p_tSys, deviceNetworkInfo_t *p_tDev)
{
  // checks for SD Card presence and type

  short timeout = 0;
  while (!SD.begin())
  {
    if (timeout > 4)
    { // errors after 10 seconds
      log_e("No SD Card detected! No internet connection possible!\n");
      vMsp_sendNetworkDataToDisplay(p_tDev, p_tSys, DISP_EVENT_SD_CARD_NOT_PRESENT);

      return false;
    }
    delay(1000); // giving it some time to detect the card properly
    timeout++;
  }
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_MMC)
  {
    log_v("SD Card type: MMC");
  }
  else if (cardType == CARD_SD)
  {
    log_v("SD Card type: SD");
  }
  else if (cardType == CARD_SDHC)
  {
    log_v("SD Card type: SDHC");
  }
  else
  {
    log_e("Unidentified Card type, format the SD Card!  No internet connection possible!\n");
    vMsp_sendNetworkDataToDisplay(p_tDev, p_tSys, DISP_EVENT_SD_CARD_FORMAT);
    return false;
  }
  delay(300);
  log_v("SD Card size: %lluMB\n", SD.cardSize() / (1024 * 1024));
  return true;
}

/*********************************************************
 * @brief parse configuratuion
 *
 * @param fl
 * @param p_tDev
 * @param p_tData
 * @param pDev
 * @param sysStat
 * @param p_tSysData
 * @return uint8_t
 *********************************************************/
static uint8_t parseConfig(File fl, deviceNetworkInfo_t *p_tDev, sensorData_t *p_tData, deviceMeasurement_t *pDev, systemStatus_t *sysStat, systemData_t *p_tSysData)
{ // parses the JSON configuration file on the SD Card

  uint8_t outcome = true;
  uint8_t have_ssid = false;

  // Read the entire file into a string
  String jsonString = fl.readString();
  fl.close();

  // Parse JSON
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, jsonString);

  if (error)
  {
    log_e("Failed to parse config JSON: %s", error.c_str());
    return false;
  }

  JsonObject config = doc[JSON_CONFIG_SECTION];
  if (!config)
  {
    log_e("Missing 'config' section in JSON");
    return false;
  }

  // Parse SSID
  if (!config[JSON_KEY_SSID].isNull())
  {
    p_tDev->ssid = config[JSON_KEY_SSID].as<String>();
    if (p_tDev->ssid.length() > 0)
    {
      log_i("ssid = *%s*", p_tDev->ssid.c_str());
      have_ssid = true;
    }
    else
    {
      log_i("SSID value is empty");
    }
  }
  else
  {
    log_e("Missing SSID in config!");
    outcome = false;
  }

  // Parse Password
  if (!config[JSON_KEY_PASSWORD].isNull())
  {
    p_tDev->passw = config[JSON_KEY_PASSWORD].as<String>();
    log_i("passw = *%s*", p_tDev->passw.c_str());
  }
  else
  {
    log_e("Missing PASSWORD in config!");
  }

  // Parse Device ID
  if (!config[JSON_KEY_DEVICE_ID].isNull())
  {
    p_tDev->deviceid = config[JSON_KEY_DEVICE_ID].as<String>();
    if (p_tDev->deviceid.length() == 0)
    {
      log_e("DEVICEID value is empty!");
      outcome = false;
    }
    else
    {
      log_i("deviceid = *%s*", p_tDev->deviceid.c_str());
    }
  }
  else
  {
    log_e("Missing DEVICEID in config!");
    outcome = false;
  }

  // Parse WiFi Power
  if (!config[JSON_KEY_WIFI_POWER].isNull())
  {
    String wifiPowerStr = config[JSON_KEY_WIFI_POWER].as<String>();
    if (wifiPowerStr == "-1dBm")
    {
      log_i("Wifi power is set to POWER_MINUS_1_dBm");
      p_tDev->wifipow = WIFI_POWER_MINUS_1dBm;
    }
    else if (wifiPowerStr == "2dBm")
    {
      log_i("Wifi power is set to POWER_2dBm");
      p_tDev->wifipow = WIFI_POWER_2dBm;
    }
    else if (wifiPowerStr == "5dBm")
    {
      log_i("Wifi power is set to POWER_5dBm");
      p_tDev->wifipow = WIFI_POWER_5dBm;
    }
    else if (wifiPowerStr == "7dBm")
    {
      log_i("Wifi power is set to POWER_7dBm");
      p_tDev->wifipow = WIFI_POWER_7dBm;
    }
    else if (wifiPowerStr == "8.5dBm")
    {
      log_i("Wifi power is set to POWER_8_5dBm");
      p_tDev->wifipow = WIFI_POWER_8_5dBm;
    }
    else if (wifiPowerStr == "11dBm")
    {
      log_i("Wifi power is set to POWER_11dBm");
      p_tDev->wifipow = WIFI_POWER_11dBm;
    }
    else if (wifiPowerStr == "13dBm")
    {
      log_i("Wifi power is set to POWER_13dBm");
      p_tDev->wifipow = WIFI_POWER_13dBm;
    }
    else if (wifiPowerStr == "15dBm")
    {
      log_i("Wifi power is set to POWER_15dBm");
      p_tDev->wifipow = WIFI_POWER_15dBm;
    }
    else if (wifiPowerStr == "17dBm")
    {
      log_i("Wifi power is set to POWER_17dBm");
      p_tDev->wifipow = WIFI_POWER_17dBm;
    }
    else if (wifiPowerStr == "18.5dBm")
    {
      log_i("Wifi power is set to POWER_18_5dBm");
      p_tDev->wifipow = WIFI_POWER_18_5dBm;
    }
    else if (wifiPowerStr == "19dBm")
    {
      log_i("Wifi power is set to POWER_19dBm");
      p_tDev->wifipow = WIFI_POWER_19dBm;
    }
    else if (wifiPowerStr == "19.5dBm")
    {
      log_i("Wifi power is set to POWER_19_5dBm");
      p_tDev->wifipow = WIFI_POWER_19_5dBm;
    }
    else
    {
      log_i("Wifi power parameter not recognized. Falling back to 17dBm");
      p_tDev->wifipow = WIFI_POWER_17dBm;
    }
  }
  else
  {
    log_e("Missing WIFI_POWER in config. Falling back to default value (17dBm)");
    p_tDev->wifipow = WIFI_POWER_17dBm;
  }

  // Parse O3 Zero Value
  p_tData->ozoneData.o3ZeroOffset = config[JSON_KEY_O3_ZERO_VALUE] | -1;
  log_i("o3_zero_value = *%d*", p_tData->ozoneData.o3ZeroOffset);

  // Parse Average Measurements
  pDev->avg_measurements = config[JSON_KEY_AVERAGE_MEASUREMENTS] | 30;
  log_i("avgMeasure = *%d*", pDev->avg_measurements);

  // Parse Average Delay
  pDev->avg_delay = config[JSON_KEY_AVERAGE_DELAY_SECONDS] | 55;
  log_i("avgDelay = *%d*", pDev->avg_delay);

  // Parse Sea Level Altitude
  p_tData->gasData.seaLevelAltitude = config[JSON_KEY_SEA_LEVEL_ALTITUDE] | 122.0f;
  log_i("sealevelalt = *%.2f*", p_tData->gasData.seaLevelAltitude);

  // Parse Upload Server
  if (!config[JSON_KEY_UPLOAD_SERVER].isNull())
  {
    String serverStr = config[JSON_KEY_UPLOAD_SERVER].as<String>();
    if (serverStr.length() > 0)
    {
      p_tSysData->server = serverStr;
      p_tSysData->server_ok = true;
      sysStat->server_ok = true;
    }
    else
    {
#ifdef API_SERVER
      log_i("SERVER value is empty. Falling back to value defined at compile time");
#else
      log_e("SERVER value is empty!");
#endif
    }
  }
  else
  {
#ifdef API_SERVER
    log_i("Missing UPLOAD_SERVER in config. Falling back to value defined at compile time");
#else
    log_e("Missing UPLOAD_SERVER in config!");
#endif
  }
  log_i("server = *%s*", p_tSysData->server.c_str());

  // Parse MICS Calibration Values
  if (!config[JSON_KEY_MICS_CALIBRATION_VALUES].isNull())
  {
    JsonObject micsCalib = config[JSON_KEY_MICS_CALIBRATION_VALUES];
    p_tData->pollutionData.data.carbonMonoxide = micsCalib[JSON_KEY_MICS_RED] | 955.0f;
    p_tData->pollutionData.data.nitrogenDioxide = micsCalib[JSON_KEY_MICS_OX] | 900.0f;
    p_tData->pollutionData.data.ammonia = micsCalib[JSON_KEY_MICS_NH3] | 163.0f;
  }
  else
  {
    log_e("Missing MICS_CALIBRATION_VALUES in config. Using defaults");
    p_tData->pollutionData.data.carbonMonoxide = 955;
    p_tData->pollutionData.data.nitrogenDioxide = 900;
    p_tData->pollutionData.data.ammonia = 163;
  }
  log_i("MICSCal[] = *%.1f*, *%.1f*, *%.1f*", p_tData->pollutionData.data.carbonMonoxide, p_tData->pollutionData.data.nitrogenDioxide, p_tData->pollutionData.data.ammonia);

  // Parse MICS Measurement Offsets
  if (!config[JSON_KEY_MICS_MEASUREMENTS_OFFSETS].isNull())
  {
    JsonObject micsOffset = config[JSON_KEY_MICS_MEASUREMENTS_OFFSETS];
    p_tData->pollutionData.sensingResInAirOffset.redSensor = (int16_t)(micsOffset[JSON_KEY_MICS_RED] | 0);
    p_tData->pollutionData.sensingResInAirOffset.oxSensor = (int16_t)(micsOffset[JSON_KEY_MICS_OX] | 0);
    p_tData->pollutionData.sensingResInAirOffset.nh3Sensor = (int16_t)(micsOffset[JSON_KEY_MICS_NH3] | 0);
  }
  else
  {
    log_e("Missing MICS_MEASUREMENTS_OFFSETS in config. Using defaults");
    p_tData->pollutionData.sensingResInAirOffset.redSensor = 0;
    p_tData->pollutionData.sensingResInAirOffset.oxSensor = 0;
    p_tData->pollutionData.sensingResInAirOffset.nh3Sensor = 0;
  }
  log_i("MICSoffset[] = *%d*, *%d*, *%d*", p_tData->pollutionData.sensingResInAirOffset.redSensor, p_tData->pollutionData.sensingResInAirOffset.oxSensor, p_tData->pollutionData.sensingResInAirOffset.nh3Sensor);

  // Parse Compensation Factors
  if (!config[JSON_KEY_COMPENSATION_FACTORS].isNull())
  {
    JsonObject compFactors = config[JSON_KEY_COMPENSATION_FACTORS];
    p_tData->compParams.currentHumidity = compFactors[JSON_KEY_COMP_H] | 0.6f;
    p_tData->compParams.currentTemperature = compFactors[JSON_KEY_COMP_T] | 1.352f;
    p_tData->compParams.currentPressure = compFactors[JSON_KEY_COMP_P] | 0.0132f;
  }
  else
  {
    log_e("Missing COMPENSATION_FACTORS in config. Using defaults");
    p_tData->compParams.currentHumidity = 0.6f;
    p_tData->compParams.currentTemperature = 1.352f;
    p_tData->compParams.currentPressure = 0.0132f;
  }
  log_i("compensation[] = *%.3f*, *%.3f*, *%.6f*", p_tData->compParams.currentHumidity, p_tData->compParams.currentTemperature, p_tData->compParams.currentPressure);

  // Parse Use Modem
  sysStat->use_modem = config[JSON_KEY_USE_MODEM] | false;
  log_i("useModem = *%s*", (sysStat->use_modem) ? "true" : "false");

  // Parse Modem APN
  if (!config[JSON_KEY_MODEM_APN].isNull())
  {
    String apnStr = config[JSON_KEY_MODEM_APN].as<String>();
    if (apnStr.length() > 0)
    {
      p_tDev->apn = apnStr;
      log_i("modem_apn = *%s*", p_tDev->apn.c_str());
    }
    else
    {
      log_i("modem_apn is empty");
    }
  }
  else
  {
    log_e("Missing MODEM_APN in config!");
  }

  // Parse NTP Server
  if (!config[JSON_KEY_NTP_SERVER].isNull())
  {
    String ntpStr = config[JSON_KEY_NTP_SERVER].as<String>();
    if (ntpStr.length() > 0)
    {
      p_tSysData->ntp_server = ntpStr;
      log_i("ntp_server = *%s*", p_tSysData->ntp_server.c_str());
    }
    else
    {
      log_e("NTP_SERVER value is empty. Falling back to default value (pool.ntp.org)");
      p_tSysData->ntp_server = "pool.ntp.org";
    }
  }
  else
  {
    log_e("Missing NTP_SERVER in config. Falling back to default value (pool.ntp.org)");
    p_tSysData->ntp_server = "pool.ntp.org";
  }

  // Parse Timezone
  if (!config[JSON_KEY_TIMEZONE].isNull())
  {
    String timezoneStr = config[JSON_KEY_TIMEZONE].as<String>();
    if (timezoneStr.length() > 0)
    {
      p_tSysData->timezone = timezoneStr;
      log_i("timezone = *%s*", p_tSysData->timezone.c_str());
    }
    else
    {
      log_i("timezone = *%s*", p_tSysData->timezone.c_str());
    }
  }
  else
  {
    log_e("Missing TIMEZONE in config!");
  }

  // Parse Firmware Auto Upgrade
  sysStat->fwAutoUpgrade = config[JSON_KEY_FW_AUTO_UPGRADE] | false;
  log_i("fwAutoUpgrade = *%s*", (sysStat->fwAutoUpgrade) ? "true" : "false");

  return outcome;
}

/***************************************************************
 * @brief check configuration
 *
 * @param configpath
 * @param p_tDev
 * @param p_tData
 * @param pDev
 * @param p_tSys
 * @param p_tSysData
 * @return uint8_t
 ***************************************************************/
uint8_t checkConfig(const char *configpath, deviceNetworkInfo_t *p_tDev, sensorData_t *p_tData, deviceMeasurement_t *pDev, systemStatus_t *p_tSys, systemData_t *p_tSysData)
{ // verifies the existance of the configuration file, creates the file if not found

  File cfgfile;

  if (SD.exists(configpath))
  {
    cfgfile = SD.open(configpath, FILE_READ); // open read only
    log_i("Found config file. Parsing...\n");

    if (parseConfig(cfgfile, p_tDev, p_tData, pDev, p_tSys, p_tSysData))
    {
      return true;
    }
    else
    {
      log_e("Error parsing config file! No network configuration!\n");
      vMsp_sendNetworkDataToDisplay(p_tDev, p_tSys, DISP_EVENT_SD_CARD_CONFIG_ERROR);

      return false;
    }
  }
  else
  {
    log_e("Couldn't find config file! Creating a new one with template...");
    vMsp_sendNetworkDataToDisplay(p_tDev, p_tSys, DISP_EVENT_SD_CARD_CONFIG_CREATE);
    cfgfile = SD.open(configpath, FILE_WRITE); // open r/w

    if (cfgfile)
    {
      // Create JSON template for default config file
      JsonDocument doc;

      // Create config section
      JsonObject config = doc[JSON_CONFIG_SECTION].to<JsonObject>();
      config[JSON_KEY_SSID] = "";
      config[JSON_KEY_PASSWORD] = "";
      config[JSON_KEY_DEVICE_ID] = "";
      config[JSON_KEY_WIFI_POWER] = "17dBm";
      config[JSON_KEY_O3_ZERO_VALUE] = p_tData->ozoneData.o3ZeroOffset;
      config[JSON_KEY_AVERAGE_MEASUREMENTS] = pDev->avg_measurements;
      config[JSON_KEY_AVERAGE_DELAY_SECONDS] = pDev->avg_delay;
      config[JSON_KEY_SEA_LEVEL_ALTITUDE] = p_tData->gasData.seaLevelAltitude;
      config[JSON_KEY_UPLOAD_SERVER] = "";

      // MICS calibration values
      JsonObject micsCalib = config[JSON_KEY_MICS_CALIBRATION_VALUES].to<JsonObject>();
      micsCalib[JSON_KEY_MICS_RED] = p_tData->pollutionData.data.carbonMonoxide;
      micsCalib[JSON_KEY_MICS_OX] = p_tData->pollutionData.data.nitrogenDioxide;
      micsCalib[JSON_KEY_MICS_NH3] = p_tData->pollutionData.data.ammonia;

      // MICS measurement offsets
      JsonObject micsOffset = config[JSON_KEY_MICS_MEASUREMENTS_OFFSETS].to<JsonObject>();
      micsOffset[JSON_KEY_MICS_RED] = p_tData->pollutionData.sensingResInAirOffset.redSensor;
      micsOffset[JSON_KEY_MICS_OX] = p_tData->pollutionData.sensingResInAirOffset.oxSensor;
      micsOffset[JSON_KEY_MICS_NH3] = p_tData->pollutionData.sensingResInAirOffset.nh3Sensor;

      // Compensation factors
      JsonObject compFactors = config[JSON_KEY_COMPENSATION_FACTORS].to<JsonObject>();
      compFactors[JSON_KEY_COMP_H] = p_tData->compParams.currentHumidity;
      compFactors[JSON_KEY_COMP_T] = p_tData->compParams.currentTemperature;
      compFactors[JSON_KEY_COMP_P] = p_tData->compParams.currentPressure;

      config[JSON_KEY_USE_MODEM] = p_tSys->use_modem;
      config[JSON_KEY_MODEM_APN] = "";
      config[JSON_KEY_NTP_SERVER] = "pool.ntp.org";
      config[JSON_KEY_TIMEZONE] = "CET-1CEST";
      config[JSON_KEY_FW_AUTO_UPGRADE] = false;

      // Create help section
      JsonObject help = doc[JSON_HELP_SECTION].to<JsonObject>();
      help[JSON_KEY_WIFI_POWER] = "Accepted values: -1, 2, 5, 7, 8.5, 11, 13, 15, 17, 18.5, 19, 19.5 dBm";
      help[JSON_KEY_AVERAGE_MEASUREMENTS] = "Accepted values: 1, 2, 3, 4, 5, 6, 10, 12, 15, 20, 30, 60";
      help[JSON_KEY_SEA_LEVEL_ALTITUDE] = "Value in meters, must be changed according to device location. 122.0 meters is the average altitude in Milan, Italy";
      help[JSON_KEY_TIMEZONE] = "Standard tz timezone definition. More details at https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html";

      // Serialize and write JSON to file
      serializeJsonPretty(doc, cfgfile);
      log_i("New config file with template created!\n");
      cfgfile.close();
      vMsp_sendNetworkDataToDisplay(p_tDev, p_tSys, DISP_EVENT_SD_CARD_CONFIG_INS_DATA);
    }
    else
    {
      log_e("Error writing to SD Card!\n");
      vMsp_sendNetworkDataToDisplay(p_tDev, p_tSys, DISP_EVENT_SD_CARD_WRITE_DATA);
    }

    return false;
  }
}

/*********************************************************
 * @brief check log file
 *
 * @param p_tDev
 * @return uint8_t
 *********************************************************/
uint8_t uHalSdcard_checkLogFile(deviceNetworkInfo_t *p_tDev)
{ // verifies the existance of the csv log using the logpath var, creates the file if not found

  if (!SD.exists(p_tDev->logpath))
  {
    log_e("Couldn't find log file. Creating a new one...\n");
    File filecsv = SD.open(p_tDev->logpath, FILE_WRITE);

    if (filecsv)
    { // Creating logfile and adding header string
      String heads = "sent_ok?;recordedAt;date;time;temp;hum;PM1;PM2_5;PM10;pres;radiation;nox;co;nh3;o3;voc;msp";
      filecsv.println(heads);
      log_i("Log file created!\n");
      filecsv.close();
      return true;
    }

    log_e("Error creating log file!\n");
    return false;
  }

  log_i("Log file present!\n");
  return true;
}

/****************************************************************************
 * @brief add to log
 *
 * @param path
 * @param oldpath
 * @param errpath
 * @param message
 * @return uint8_t
 ***************************************************************************/
uint8_t addToLog(const char *path, const char *oldpath, const char *errpath, String *message)
{ // adds new line to the log file at the top, after the header lines

  String temp = "";
  log_v("Log file is located at: %s\n", path);
  log_v("Old path is: %s\n", oldpath);
  if (!SD.exists(oldpath))
  {
    SD.rename(path, oldpath);
  }
  else
  {
    if (SD.exists(path))
      SD.rename(path, errpath);
    log_e("An error occurred, resuming logging from the old log...\n");
  }
  File oldfile = SD.open(oldpath, FILE_READ); // opening the renamed log
  if (!oldfile)
  {
    log_e("Error opening the renamed the log file!");
    return false;
  }
  File logfile = SD.open(path, FILE_WRITE); // recreates empty logfile
  if (!logfile)
  {
    log_e("Error recreating the log file!");
    return false;
  }
  temp = oldfile.readStringUntil('\r'); // reads until CR character
  logfile.println(temp);
  oldfile.readStringUntil('\n'); // consumes LF character (uses DOS-style CRLF)
  logfile.println(*message);     // printing the new string, only once and after the header
  log_v("New line added!\n");
  while (oldfile.available())
  {                                       // copy the old log file with new string added
    temp = oldfile.readStringUntil('\r'); // reads until CR character
    logfile.println(temp);
    oldfile.readStringUntil('\n'); // consumes LF character (uses DOS-style CRLF)
  }
  oldfile.close();
  logfile.close();
  SD.remove(oldpath); // deleting old log

  return true;
}

/*******************************************************************************
 * @brief log to SD card
 *
 * @param data
 * @param p_tSysData
 * @param p_tSys
 * @param p_tData
 * @param p_tDev
 ******************************************************************************/
void vHalSdcard_logToSD(send_data_t *data, systemData_t *p_tSysData, systemStatus_t *p_tSys, sensorData_t *p_tData, deviceNetworkInfo_t *p_tDev)
{ // builds a new logfile line and calls addToLog() (using logpath global var) to add it

  log_i("Logging data to the .csv on the SD Card...\n");

  strftime(p_tSysData->Date, sizeof(p_tSysData->Date), "%d/%m/%Y", &data->sendTimeInfo); // Formatting date as DD/MM/YYYY
  strftime(p_tSysData->Time, sizeof(p_tSysData->Time), "%T", &data->sendTimeInfo);       // Formatting time as HH:MM:SS

  String logvalue = "";
  char timeFormat[29] = {0};
  if (p_tSys->datetime)
    strftime(timeFormat, sizeof(timeFormat), "%Y-%m-%dT%T.000Z", &data->sendTimeInfo); // formatting date&time in TZ format

  // Data is layed out as follows:
  // "sent_ok?;recordedAt;date;time;temp;hum;PM1;PM2_5;PM10;pres;radiation;nox;co;nh3;o3;voc;msp"

  logvalue += (p_tSysData->sent_ok) ? "OK" : "ERROR";
  logvalue += ";";
  logvalue += String(timeFormat);
  logvalue += ";";
  logvalue += String(p_tSysData->Date);
  logvalue += ";";
  logvalue += String(p_tSysData->Time);
  logvalue += ";";
  if (p_tData->status.BME680Sensor)
  {
    logvalue += vGeneric_floatToComma(data->temp);
  }
  logvalue += ";";
  if (p_tData->status.BME680Sensor)
  {
    logvalue += vGeneric_floatToComma(data->hum);
  }
  logvalue += ";";
  if (p_tData->status.PMS5003Sensor)
  {
    logvalue += String(data->PM1);
  }
  logvalue += ";";
  if (p_tData->status.PMS5003Sensor)
  {
    logvalue += String(data->PM25);
  }
  logvalue += ";";
  if (p_tData->status.PMS5003Sensor)
  {
    logvalue += String(data->PM10);
  }
  logvalue += ";";
  if (p_tData->status.BME680Sensor)
  {
    logvalue += vGeneric_floatToComma(data->pre);
  }
  logvalue += ";";
  logvalue += ";"; // for "radiation"
  if (p_tData->status.MICS6814Sensor)
  {
    logvalue += vGeneric_floatToComma(data->MICS_NO2);
  }
  logvalue += ";";
  if (p_tData->status.MICS6814Sensor)
  {
    logvalue += vGeneric_floatToComma(data->MICS_CO);
  }
  logvalue += ";";
  if (p_tData->status.MICS6814Sensor)
  {
    logvalue += vGeneric_floatToComma(data->MICS_NH3);
  }
  logvalue += ";";
  if (p_tData->status.O3Sensor)
  {
    logvalue += vGeneric_floatToComma(data->ozone);
  }
  logvalue += ";";
  if (p_tData->status.BME680Sensor)
  {
    logvalue += vGeneric_floatToComma(data->VOC);
  }
  logvalue += ";";
  logvalue += String(data->MSP);

  String oldlogpath = p_tDev->logpath + ".old";
  String errorpath = "logerror_" + String(timeFormat) + ".txt";

  if (addToLog(p_tDev->logpath.c_str(), oldlogpath.c_str(), errorpath.c_str(), &logvalue))
  {
    log_i("SD Card log file updated successfully!\n");
  }
  else
  {
    log_e("Error updating SD Card log file!\n");
    vMsp_sendNetworkDataToDisplay(p_tDev, p_tSys, DISP_EVENT_SD_CARD_LOG_ERROR);
  }
}

/******************************************************
 * @brief read SD card
 *
 * @param p_tSys
 * @param p_tDev
 * @param p_tData
 * @param pDev
 * @param p_tSysData
 *****************************************************/
void vHalSdcard_readSD(systemStatus_t *p_tSys, deviceNetworkInfo_t *p_tDev, sensorData_t *p_tData, deviceMeasurement_t *pDev, systemData_t *p_tSysData)
{

  log_i("Initializing SD Card...\n");
  vMsp_sendNetworkDataToDisplay(p_tDev, p_tSys, DISP_EVENT_SD_CARD_INIT);
  p_tSys->sdCard = initializeSD(p_tSys, p_tDev);
  if (p_tSys->sdCard)
  {
    log_i("SD Card ok! Reading configuration...\n");
    vMsp_sendNetworkDataToDisplay(p_tDev, p_tSys, DISP_EVENT_CONFIG_READ);
    p_tSys->configuration = checkConfig(CONFIG_PATH, p_tDev, p_tData, pDev, p_tSys, p_tSysData);
    if (p_tSys->server_ok)
    {
      log_e("No server URL defined. Can't upload data!\n");
      vMsp_sendNetworkDataToDisplay(p_tDev, p_tSys, DISP_EVENT_URL_UPLOAD_STAT);
    }
    // if (avg_delay < 50) {
    //   log_e("AVG_DELAY should be at least 50 seconds! Setting to 50...\n");
    //   vHalDisplay_drawTwoLines("AVG_DELAY less than 50!", "Setting to 50...", 5);
    //   avg_delay = 50; // must be at least 45 for PMS5003 compensation routine, 5 seconds extra for reading cycle messages
    // }
    // setting the logpath variable
    p_tDev->logpath = "/log_" + p_tDev->deviceid + "_" + p_tSysData->ver + ".csv";
    // checking logfile existance
    uHalSdcard_checkLogFile(p_tDev);
  }
}

/********************************************************
 * @brief periodic SD card presence check
 *
 * @param p_tSys system status structure
 * @param p_tDev device network info structure
 * @return uint8_t current SD card presence status
 ********************************************************/
uint8_t vHalSdcard_periodicCheck(systemStatus_t *p_tSys, deviceNetworkInfo_t *p_tDev)
{
  static uint8_t previousSdStatus = 255; // Use 255 as uninitialized marker
  uint8_t currentSdStatus = false;
  

  // Initialize previousSdStatus based on actual system status on first call
  if (previousSdStatus == 255)
  {
    previousSdStatus = p_tSys->sdCard;
  }

  // Quick check without lengthy initialization
  uint8_t cardType = SD.cardType();
  if (cardType != CARD_NONE)
  {
    currentSdStatus = true;
    log_v("SD Card periodic check: Present");
  }
  else
  {
    // Only try re-initialization if we think the card was previously present
    // This avoids unnecessary SD.begin() calls that might interfere with operations
    if (previousSdStatus == true)
    {
      // Try to re-initialize if card seems missing but was previously present
      if (SD.begin())
      {
        cardType = SD.cardType();
        if (cardType != CARD_NONE)
        {
          currentSdStatus = true;
          log_v("SD Card periodic check: Present (after re-init)");
        }
        else
        {
          currentSdStatus = false;
          log_v("SD Card periodic check: Not present");
        }
      }
      else
      {
        currentSdStatus = false;
        log_v("SD Card periodic check: Not present (re-init failed)");
      }
    }
    else
    {
      currentSdStatus = false;
      log_v("SD Card periodic check: Not present (no re-init attempted)");
    }
  }

  // Update system status and log changes
  if (currentSdStatus != previousSdStatus)
  {
    if (currentSdStatus)
    {
      log_i("SD Card detected - card was inserted");
      vMsp_sendNetworkDataToDisplay(p_tDev, p_tSys, DISP_EVENT_SD_CARD_INIT);
    }
    else
    {
      log_w("SD Card removed - card is no longer present");
      vMsp_sendNetworkDataToDisplay(p_tDev, p_tSys, DISP_EVENT_SD_CARD_NOT_PRESENT);
    }
    previousSdStatus = currentSdStatus;
  }

  // Update system status
  p_tSys->sdCard = currentSdStatus;


  return currentSdStatus;
}
