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

#include "sdcard.h"
#include "generic_functions.h"
#include "display_task.h"
#include "display.h"
#include "network.h"
#include "mspOs.h"

// --defines --
#define LINES 16

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
{ // parses the configuration file on the SD Card

  uint8_t outcome = true;
  String command[LINES];
  short line_cnt = 0;
  uint8_t have_ssid = false;
  String temp;
  int i = 0;
  unsigned long lastpos = 0;
  while (fl.available() && i < LINES)
  { // Storing the config file in a string array
    fl.seek(lastpos);
    if (i == 0)
      temp = fl.readStringUntil('#');
    command[i] = fl.readStringUntil(';');
    temp = fl.readStringUntil('#');
    log_d("Line number %d: %s", i + 1, command[i].c_str());
    lastpos = fl.position();
    i++;
  }
  fl.close();
  // Importing variables from the string array.
  // ssid
  if (command[line_cnt].startsWith("ssid", 0))
  {
    p_tDev->ssid = command[line_cnt].substring(command[line_cnt].indexOf('=') + 1, command[line_cnt].length());
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
    log_e("Error parsing SSID line!");
    outcome = false;
  }
  line_cnt++;
  // passw
  if (command[line_cnt].startsWith("password", 0))
  {
    p_tDev->passw = command[line_cnt].substring(command[line_cnt].indexOf('=') + 1, command[line_cnt].length());
    log_i("passw = *%s*", p_tDev->passw.c_str());
  }
  else
  {
    log_e("Error parsing PASSWORD line!");
  }
  line_cnt++;
  // deviceid
  if (command[line_cnt].startsWith("device_id", 0))
  {
    p_tDev->deviceid = command[line_cnt].substring(command[line_cnt].indexOf('=') + 1, command[line_cnt].length());
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
    log_e("Error parsing DEVICE_ID line!");
    outcome = false;
  }
  line_cnt++;
  // wifipow
  if (command[line_cnt].startsWith("wifi_power", 0))
  {
    temp = "";
    temp = command[line_cnt].substring(command[line_cnt].indexOf('=') + 1, command[line_cnt].length());
    if (temp.indexOf("19.5dBm") == 0)
    {
      p_tDev->wifipow = WIFI_POWER_19_5dBm;
      log_i("wifipow = *WIFI_POWER_19_5dBm*");
    }
    else if (temp.indexOf("19dBm") == 0)
    {
      p_tDev->wifipow = WIFI_POWER_19dBm;
      log_i("wifipow = *WIFI_POWER_19dBm*");
    }
    else if (temp.indexOf("18.5dBm") == 0)
    {
      p_tDev->wifipow = WIFI_POWER_18_5dBm;
      log_i("wifipow = *WIFI_POWER_18_5dBm*");
    }
    else if (temp.indexOf("17dBm") == 0)
    {
      p_tDev->wifipow = WIFI_POWER_17dBm;
      log_i("wifipow = *WIFI_POWER_17dBm*");
    }
    else if (temp.indexOf("15dBm") == 0)
    {
      p_tDev->wifipow = WIFI_POWER_15dBm;
      log_i("wifipow = *WIFI_POWER_15dBm*");
    }
    else if (temp.indexOf("13dBm") == 0)
    {
      p_tDev->wifipow = WIFI_POWER_13dBm;
      log_i("wifipow = *WIFI_POWER_13dBm*");
    }
    else if (temp.indexOf("11dBm") == 0)
    {
      p_tDev->wifipow = WIFI_POWER_11dBm;
      log_i("wifipow = *WIFI_POWER_11dBm*");
    }
    else if (temp.indexOf("8.5dBm") == 0)
    {
      p_tDev->wifipow = WIFI_POWER_8_5dBm;
      log_i("wifipow = *WIFI_POWER_8_5dBm*");
    }
    else if (temp.indexOf("7dBm") == 0)
    {
      p_tDev->wifipow = WIFI_POWER_7dBm;
      log_i("wifipow = *WIFI_POWER_7dBm*");
    }
    else if (temp.indexOf("5dBm") == 0)
    {
      p_tDev->wifipow = WIFI_POWER_5dBm;
      log_i("wifipow = *WIFI_POWER_5dBm*");
    }
    else if (temp.indexOf("2dBm") == 0)
    {
      p_tDev->wifipow = WIFI_POWER_2dBm;
      log_i("wifipow = *WIFI_POWER_2dBm*");
    }
    else if (temp.indexOf("-1dBm") == 0)
    {
      p_tDev->wifipow = WIFI_POWER_MINUS_1dBm;
      log_i("wifipow = *WIFI_POWER_MINUS_1dBm*");
    }
    else
    {
      log_e("Invalid WIFIPOW value. Falling back to default value");
    }
  }
  else
  {
    log_e("Error parsing WIFI_POWER line. Falling back to default value");
  }
  line_cnt++;
  // o3zeroval
  if (command[line_cnt].startsWith("o3_zero_value", 0))
  {
    p_tData->ozoneData.o3ZeroOffset = command[line_cnt].substring(command[line_cnt].indexOf('=') + 1, command[line_cnt].length()).toInt();
  }
  else
  {
    log_e("Error parsing O3_ZERO_VALUE line. Falling back to default value");
  }
  log_i("o3zeroval = *%d*", p_tData->ozoneData.o3ZeroOffset);
  line_cnt++;
  // avg_measurements
  if (command[line_cnt].startsWith("average_measurements", 0))
  {
    pDev->avg_measurements = command[line_cnt].substring(command[line_cnt].indexOf('=') + 1, command[line_cnt].length()).toInt();
  }
  else
  {
    log_e("Error parsing AVERAGE_MEASUREMENTS line. Falling back to default value");
  }
  log_i("avg_measurements = *%d*", pDev->avg_measurements);
  line_cnt++;
  // avg_delay
  if (command[line_cnt].startsWith("average_delay(seconds)", 0))
  {
    pDev->avg_delay = command[line_cnt].substring(command[line_cnt].indexOf('=') + 1, command[line_cnt].length()).toInt();
  }
  else
  {
    log_e("Error parsing AVERAGE_DELAY(SECONDS) line. Falling back to default value");
  }
  log_i("avg_delay = *%d*", pDev->avg_delay);
  line_cnt++;
  // sealevelalt
  if (command[line_cnt].startsWith("sea_level_altitude", 0))
  {
    p_tData->gasData.seaLevelAltitude = command[line_cnt].substring(command[line_cnt].indexOf('=') + 1, command[line_cnt].length()).toFloat();
  }
  else
  {
    log_e("Error parsing SEA_LEVEL_ALTITUDE line. Falling back to default value");
  }
  log_i("sealevelalt = *%.2f*", p_tData->gasData.seaLevelAltitude);
  line_cnt++;
  // server
  if (command[line_cnt].startsWith("upload_server", 0))
  {
    temp = "";
    temp = command[line_cnt].substring(command[line_cnt].indexOf('=') + 1, command[line_cnt].length());
    if (temp.length() > 0)
    {
      p_tSysData->server = temp;
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
    log_i("Error parsing UPLOAD_SERVER line. Falling back to value defined at compile time");
#else
    log_e("Error parsing UPLOAD_SERVER line!");
#endif
  }
  log_i("server = *%s*", p_tSysData->server.c_str());
  line_cnt++;
  // mics_calibration_values
  if (command[line_cnt].startsWith("mics_calibration_values", 0))
  {
    p_tData->pollutionData.data.carbonMonoxide = command[line_cnt].substring(command[line_cnt].indexOf("RED:") + 4, command[line_cnt].indexOf(",OX:")).toInt();
    p_tData->pollutionData.data.nitrogenDioxide = command[line_cnt].substring(command[line_cnt].indexOf(",OX:") + 4, command[line_cnt].indexOf(",NH3:")).toInt();
    p_tData->pollutionData.data.ammonia = command[line_cnt].substring(command[line_cnt].indexOf(",NH3:") + 5, command[line_cnt].length()).toInt();
  }
  else
  {
    log_e("Error parsing MICS_CALIBRATION_VALUES line. Falling back to default value");
  }
  log_i("MICSCal[] = *%d*, *%d*, *%d*", p_tData->pollutionData.data.carbonMonoxide, p_tData->pollutionData.data.nitrogenDioxide, p_tData->pollutionData.data.ammonia);
  line_cnt++;
  // mics_measurements_offsets
  if (command[line_cnt].startsWith("mics_measurements_offsets", 0))
  {
    p_tData->pollutionData.sensingResInAirOffset.redSensor = (int16_t)command[line_cnt].substring(command[line_cnt].indexOf("RED:") + 4, command[line_cnt].indexOf(",OX:")).toInt();
    p_tData->pollutionData.sensingResInAirOffset.oxSensor = (int16_t)command[line_cnt].substring(command[line_cnt].indexOf(",OX:") + 4, command[line_cnt].indexOf(",NH3:")).toInt();
    p_tData->pollutionData.sensingResInAirOffset.nh3Sensor = (int16_t)command[line_cnt].substring(command[line_cnt].indexOf(",NH3:") + 5, command[line_cnt].length()).toInt();
  }
  else
  {
    log_e("Error parsing MICS_MEASUREMENTS_OFFSETS line. Falling back to default value");
  }
  log_i("MICSOffset[] = *%d*, *%d*, *%d*", p_tData->pollutionData.sensingResInAirOffset.redSensor,
        p_tData->pollutionData.sensingResInAirOffset.oxSensor,
        p_tData->pollutionData.sensingResInAirOffset.nh3Sensor);
  line_cnt++;
  // compensation_factors
  if (command[line_cnt].startsWith("compensation_factors", 0))
  {
    p_tData->compParams.currentHumidity = command[line_cnt].substring(command[line_cnt].indexOf("compH:") + 6, command[line_cnt].indexOf(",compT:")).toFloat();
    p_tData->compParams.currentTemperature = command[line_cnt].substring(command[line_cnt].indexOf(",compT:") + 7, command[line_cnt].indexOf(",compP:")).toFloat();
    p_tData->compParams.currentPressure = command[line_cnt].substring(command[line_cnt].indexOf(",compP:") + 7, command[line_cnt].length()).toFloat();
  }
  else
  {
    log_e("Error parsing COMPENSATION_FACTORS line. Falling back to default value");
  }
  log_i("compH = *%.4f*, compT = *%.4f*, compP = *%.4f*", p_tData->compParams.currentHumidity, p_tData->compParams.currentTemperature, p_tData->compParams.currentPressure);
  line_cnt++;
  // use_modem
  if (command[line_cnt].startsWith("use_modem", 0))
  {
    temp = "";
    temp = command[line_cnt].substring(command[line_cnt].indexOf('=') + 1, command[line_cnt].length());
    if (temp.startsWith("true", 0))
      sysStat->use_modem = true;
  }
  else
  {
    log_e("Error parsing USE_MODEM line. Falling back to default value");
  }
  log_i("use_modem = *%s*", (sysStat->use_modem) ? "true" : "false");
  if (!sysStat->use_modem && !have_ssid)
    outcome = false;
  line_cnt++;
  // modem_apn
  if (command[line_cnt].startsWith("modem_apn", 0))
  {
    temp = "";
    temp = command[line_cnt].substring(command[line_cnt].indexOf('=') + 1, command[line_cnt].length());
    if (temp.length() > 0)
    {
      p_tDev->apn = temp;
    }
    else
    {
      log_i("APN value is empty");
    }
  }
  else
  {
    log_e("Error parsing MODEM_APN line!");
  }
  log_i("apn = *%s*\n", p_tDev->apn.c_str());
  if (sysStat->use_modem && p_tDev->apn.length() == 0)
    outcome = false;
  line_cnt++;
  // Timezone and NTP server
  if (command[line_cnt].startsWith("ntp_server", 0))
  {
    p_tSysData->ntp_server = command[line_cnt].substring(command[line_cnt].indexOf('=') + 1, command[line_cnt].length());
    if (p_tSysData->ntp_server.length() == 0)
    {
      log_e("NTP_SERVER value is empty! Use default value: pool.ntp.org");
    }
    else
    {
      log_i("ntp_server = *%s*", p_tSysData->ntp_server.c_str());
    }
  }
  else
  {
    log_e("Error parsing NTP_SERVER line!");
  }
  line_cnt++;
  if (command[line_cnt].startsWith("timezone", 0))
  {
    p_tSysData->timezone = command[line_cnt].substring(command[line_cnt].indexOf('=') + 1, command[line_cnt].length());
    if (p_tSysData->timezone.length() == 0)
    {
      log_e("TIMEZONE value is empty! Use default value: CET-1CEST");
    }
    else
    {
      log_i("timezone = *%s*", p_tSysData->timezone.c_str());
    }
  }
  else
  {
    log_e("Error parsing TIMEZONE line!");
  }
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
      // template for default config file
      String conftemplate =
          "#ssid=;\n"
          "#password=;\n"
          "#device_id=;\n"
          "#wifi_power=17dBm;\n"
          "#o3_zero_value=" +
          String(p_tData->ozoneData.o3ZeroOffset) + ";\n"
                                                    "#average_measurements=" +
          String(pDev->avg_measurements) + ";\n"
                                           "#average_delay(seconds)=" +
          String(pDev->avg_delay) + ";\n"
                                    "#sea_level_altitude=" +
          String(p_tData->gasData.seaLevelAltitude) + ";\n"
                                                      "#upload_server=;\n"
                                                      "#mics_calibration_values=RED:" +
          String(p_tData->pollutionData.data.carbonMonoxide) + ",OX:" + String(p_tData->pollutionData.data.nitrogenDioxide) + ",NH3:" + String(p_tData->pollutionData.data.ammonia) + ";\n"
                                                                                                                                                                                      "#mics_measurements_offsets=RED:" +
          String(p_tData->pollutionData.sensingResInAirOffset.redSensor) + ",OX:" + String(p_tData->pollutionData.sensingResInAirOffset.oxSensor) + ",NH3:" + String(p_tData->pollutionData.sensingResInAirOffset.nh3Sensor) + ";\n"
                                                                                                                                                                                                                               "#compensation_factors=compH:" +
          String(p_tData->compParams.currentHumidity, 1) + ",compT:" + String(p_tData->compParams.currentTemperature, 3) + ",compP:" + String(p_tData->compParams.currentPressure, 4) + ";\n"
                                                                                                                                                                                        "#use_modem=" +
          String(p_tSys->use_modem ? "true" : "false") + ";\n"
                                                         "#modem_apn=;\n"
                                                         "#ntp_server=pool.ntp.org;\n"
                                                         "#timezone=CET-1CEST;\n"
                                                         "\n"
                                                         "Accepted wifi_power values are: -1, 2, 5, 7, 8.5, 11, 13, 15, 17, 18.5, 19, 19.5 dBm.\n"
                                                         "\n"
                                                         "sea_level_altitude is in meters and it must be changed according to the current location of the device. 122.0 meters is the average altitude in Milan, Italy.\n"
                                                         "The timezone follows the standard definition in the tz timezone.\n"
                                                         "More details at https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html";
      cfgfile.println(conftemplate);
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
    p_tSys->configuration = checkConfig("/config_v3.txt", p_tDev, p_tData, pDev, p_tSys, p_tSysData);
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
