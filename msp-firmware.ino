/*
                        Milano Smart Park Firmware
                       Developed by Norman Mulinacci

          This code is usable under the terms and conditions of the
             GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
 */

// PMS5003 serial pins
// with WROVER module don't use UART 2 mode on pins 16 and 17: it crashes!
#define PMSERIAL_RX 14
#define PMSERIAL_TX 12

// Serial modem pins
#define MODEM_RST 4

// Select modem type
#define TINY_GSM_MODEM_SIM800

// Basic system libraries
#include <Arduino.h>
#include <FS.h>
#include <SD.h>

// Project configuration
#include "config.h"
#include <Wire.h>

#include <stdint.h>
#include <stdio.h>

// WiFi Client, NTP time management, Modem and SSL libraries
#include <WiFi.h>
#include "time.h"
#include "esp_sntp.h"
#include <TinyGsmClient.h>
#include <SSLClient.h>

// Sensors management libraries
// for BME_680
#include <bsec.h>
// for PMS5003
#include <PMS.h>
// for MICS6814
#include <MiCS6814-I2C.h>

// OLED display library
#include <U8g2lib.h>

// --local dependency includes  --
#include "shared_values.h"
#include "display.h"
#include "sensors.h"
#include "network.h"
#include "generic_functions.h"
#include "sdcard.h"
#include "trust_anchor.h"
#include "display_task.h"
#include "mspOs.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

// Hardware UART definitions. Modes: UART0=0(debug out); UART1=1; UART2=2

HardwareSerial pmsSerial(2);

// ------------------------------- DEFINES -----------------------------------------------------------------------------

// ------------------------------- INSTANCES  --------------------------------------------------------------------------

// -- BME680 sensor instance
static Bsec bme680;

// -- PMS5003 sensor instance
static PMS pms(pmsSerial);

// -- MICS6814 sensors instances
static MiCS6814 gas;

// -- Create a new state machine instance
static state_machine_t mainStateMachine;

// -- sensor data and status instance
static sensorData_t sensorData;

// -- bme 680 data instance
static bme680Data_t localData;

// -- reading status instance
static errorVars_t err;

// -- system status instance
static systemStatus_t sysStat;

// -- device network info instance
static deviceNetworkInfo_t devinfo;

// -- device measurement status
static deviceMeasurement_t measStat;

// -- time information data
static tm timeinfo;

// -- time zone and ntp server data
static systemData_t sysData;

// -- structure for PMS5003
static PMS::DATA data;

displayData_t displayData;

//---------------------------------------- FUNCTIONS ----------------------------------------------------------------------

void vMspInit_sensorStatusAndData(sensorData_t *p_tData);

void vMspInit_setDefaultNtpTimezoneStatus(systemData_t *p_tData);

void vMspInit_setDefaultSslName(systemData_t *p_tData);

void vMspInit_setApiSecSaltAndFwVer(systemData_t *p_tData);

void vMsp_updateDataAndEvent(displayEvents_t event);

void vMsp_setGpioPins(void);

void vMspInit_NetworkAndMeasInfo(void);
void vMspInit_MeasInfo(void);

void Msp_getSystemStatus(systemStatus_t *stat);

//*******************************************************************************************************************************

void Msp_getSystemStatus(systemStatus_t *stat)
{

  vMspOs_takeDataAccessMutex();

  memcpy(stat, &sysStat, sizeof(systemStatus_t));

  vMspOs_giveDataAccessMutex();
}

/******************************************************************************************************************/

//*******************************************************************************************************************************
//******************************************  S E T U P  ************************************************************************
//*******************************************************************************************************************************
void setup()
{
  memset(&mainStateMachine, 0, sizeof(mainStateMachine)); // Initialize the state machine structure
  memset(&sensorData, 0, sizeof(sensorData));
  sysData = systemData_t();
  displayData = displayData_t();

  vMsp_setGpioPins();

  vMspOs_initDataAccessMutex(); // Create the OS mutex for data access

  // init the sensor data structure /status / offset values with defualt values
  vMspInit_sensorStatusAndData(&sensorData);

  // init device network parameters
  vMspInit_NetworkAndMeasInfo();

  // init the ntp server and timezone data with default values (network task will override from SD config)
  vMspInit_setDefaultNtpTimezoneStatus(&sysData);

  // Default server name for SSL data upload
  vMspInit_setDefaultSslName(&sysData);

  // set API Secret salt and FW Version
  vMspInit_setApiSecSaltAndFwVer(&sysData);

  // Initialize the serial port and I2C for the display
  vHalDisplay_initSerialAndI2c();

  // Initialize the display task data queue
  vTaskDisplay_initDataQueue();

  // Create the display task
  vTaskDisplay_createTask();

  // BOOT STRINGS ++++++++++++++++++++++++++++++++++++++++++++++++++++++
  log_i("\nMILANO SMART PARK");
  log_i("FIRMWARE %s by Norman Mulinacci", sysData.ver.c_str());
  log_i("Refactor and optimization by AB-Engineering - https://ab-engineering.it");
  log_i("Compiled %s %s\n", __DATE__, __TIME__);

  vMsp_updateDataAndEvent(DISP_EVENT_DEVICE_BOOT);
  tTaskDisplay_sendEvent(&displayData);

#ifdef VERSION_STRING
  log_i("ver was defined at compile time.\n");
#else
  log_i("ver is the default.\n");
#endif
#ifdef API_SECRET_SALT
  log_i("api_secret_salt was defined at compile time.\n");
#else
  log_i("api_secret_salt is the default.\n");
#endif
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  // STEP 1: Single SD Card Configuration Reading
  log_i("=== STEP 1: Loading complete system configuration from SD card ===");
  vHalSdcard_readSD(&sysStat, &devinfo, &sensorData, &measStat, &sysData);

  // STEP 2: Fill system configuration with SD data or defaults
  log_i("=== STEP 2: Configuring system with loaded data or defaults ===");
  vMspInit_configureSystemFromSD(&sysData, &sysStat, &devinfo, &measStat);

  // Debug: Check final configuration status
  log_i("Final Configuration Status:");
  log_i("  SD Card: %s", sysStat.sdCard ? "OK" : "FAILED");
  log_i("  Config File: %s", sysStat.configuration ? "OK" : "FAILED");
  log_i("  WiFi SSID: %s", devinfo.ssid.c_str());
  log_i("  Server: %s", sysData.server.c_str());
  log_i("  Avg Measurements: %d", measStat.avg_measurements);
  log_i("  Use Modem: %s", sysStat.use_modem ? "YES" : "NO");

  // STEP 3: Start network task with complete configuration
  log_i("=== STEP 3: Starting network task ===");
  createNetworkEvents();
  initSendDataOp(&sysData, &sysStat, &devinfo);

  // Wait for network task to initialize
  vTaskDelay(pdMS_TO_TICKS(2000));

  measStat.max_measurements = measStat.avg_measurements; /*!< fill the max_measurements with the number set by the user */

  //++++++++++++++++ DETECT AND INIT SENSORS ++++++++++++++++++++++++++++++
  log_i("Detecting and initializing sensors...\n");

  // BME680 +++++++++++++++++++++++++++++++++++++
  vMsp_updateDataAndEvent(DISP_EVENT_BME680_SENSOR_INIT);
  tTaskDisplay_sendEvent(&displayData);

  bme680.begin(BME68X_I2C_ADDR_HIGH, Wire);
  if (tHalSensor_checkBMESensor(&bme680))
  {
    log_i("BME680 sensor detected, initializing...\n");
    bsec_virtual_sensor_t sensor_list[] = {
        BSEC_OUTPUT_RAW_TEMPERATURE,
        BSEC_OUTPUT_RAW_PRESSURE,
        BSEC_OUTPUT_RAW_HUMIDITY,
        BSEC_OUTPUT_RAW_GAS,
        BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
        BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
    };
    bme680.updateSubscription(sensor_list, sizeof(sensor_list) / sizeof(sensor_list[0]), BSEC_SAMPLE_RATE_LP);
    sensorData.status.BME680Sensor = true;
    vMsp_updateDataAndEvent(DISP_EVENT_BME680_SENSOR_OKAY);
    tTaskDisplay_sendEvent(&displayData);
  }
  else
  {
    log_e("BME680 sensor not detected!\n");
    vMsp_updateDataAndEvent(DISP_EVENT_BME680_SENSOR_ERR);
    tTaskDisplay_sendEvent(&displayData);
  }
  //+++++++++++++++++++++++++++++++++++++++++++++

  // PMS5003 ++++++++++++++++++++++++++++++++++++
  pmsSerial.begin(9600, SERIAL_8N1, PMSERIAL_RX, PMSERIAL_TX); // baud, type, ESP_RX, ESP_TX
  delay(1500);
  vMsp_updateDataAndEvent(DISP_EVENT_PMS5003_SENSOR_INIT);
  tTaskDisplay_sendEvent(&displayData);

  pms.wakeUp(); // Waking up sensor after sleep
  delay(1500);
  if (pms.readUntil(data))
  {
    log_i("PMS5003 sensor detected, initializing...\n");
    sensorData.status.PMS5003Sensor = true;
    vMsp_updateDataAndEvent(DISP_EVENT_PMS5003_SENSOR_OKAY);
    tTaskDisplay_sendEvent(&displayData);
    pms.sleep(); // Putting sensor to sleep
  }
  else
  {
    log_e("PMS5003 sensor not detected!\n");
    vMsp_updateDataAndEvent(DISP_EVENT_PMS5003_SENSOR_ERR);
    tTaskDisplay_sendEvent(&displayData);
  }
  //++++++++++++++++++++++++++++++++++++++++++++++

  // MICS6814 ++++++++++++++++++++++++++++++++++++
  vMsp_updateDataAndEvent(DISP_EVENT_MICS6814_SENSOR_INIT);
  tTaskDisplay_sendEvent(&displayData);

  if (gas.begin())
  { // Connect to sensor using default I2C address (0x04)
    log_i("MICS6814 sensor detected, initializing...\n");
    sensorData.status.MICS6814Sensor = true;
    gas.powerOn(); // turn on heating element and led
    gas.ledOn();
    vMsp_updateDataAndEvent(DISP_EVENT_MICS6814_SENSOR_OKAY);
    tTaskDisplay_sendEvent(&displayData);

    sensorR0Value_t r0Values;
    r0Values.redSensor = gas.getBaseResistance(CH_RED);
    r0Values.oxSensor = gas.getBaseResistance(CH_OX);
    r0Values.nh3Sensor = gas.getBaseResistance(CH_NH3);

    if (tHalSensor_checkMicsValues(&sensorData, &r0Values) == STATUS_OK)
    {
      log_i("MICS6814 R0 values are already as default!\n");
      vMsp_updateDataAndEvent(DISP_EVENT_MICS6814_VALUES_OKAY);
      tTaskDisplay_sendEvent(&displayData);
    }
    else
    {
      log_i("Setting MICS6814 R0 values as default... ");
      vMsp_updateDataAndEvent(DISP_EVENT_MICS6814_DEF_SETTING);
      tTaskDisplay_sendEvent(&displayData);

      vHalSensor_writeMicsValues(&sensorData);

      vMsp_updateDataAndEvent(DISP_EVENT_MICS6814_DONE);
      tTaskDisplay_sendEvent(&displayData);
      log_i("Done!\n");
    }
    gas.setOffsets(&sensorData.pollutionData.sensingResInAirOffset.redSensor);
  }
  else
  {
    log_e("MICS6814 sensor not detected!\n");
    vMsp_updateDataAndEvent(DISP_EVENT_MICS6814_SENSOR_ERR);
    tTaskDisplay_sendEvent(&displayData);
  }
  //+++++++++++++++++++++++++++++++++++++++++++++++++++

  // O3 ++++++++++++++++++++++++++++++++++++++++++

  vMsp_updateDataAndEvent(DISP_EVENT_O3_SENSOR_INIT);
  tTaskDisplay_sendEvent(&displayData);

  if (!tHalSensor_isAnalogO3Connected())
  {
    log_e("O3 sensor not detected!\n");
    vMsp_updateDataAndEvent(DISP_EVENT_O3_SENSOR_ERR);
    tTaskDisplay_sendEvent(&displayData);
  }
  else
  {
    log_i("O3 sensor detected, running...\n");
    sensorData.status.O3Sensor = true;
    vMsp_updateDataAndEvent(DISP_EVENT_O3_SENSOR_OKAY);
    tTaskDisplay_sendEvent(&displayData);
  }
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++

  if (sensorData.status.PMS5003Sensor)
  {
    measStat.additional_delay = PMS_PREHEAT_TIME_IN_SEC;
  }
  else
  {
    measStat.additional_delay = 0; /*!< No additional delay if PMS is not running */
  }

  // Always use 1-minute intervals for measurements
  measStat.delay_between_measurements = SEC_IN_MIN;

  // Calculate transmission intervals based on avg_measurements
  if (measStat.max_measurements > 0 && (60 % measStat.max_measurements) == 0)
  {
    int transmission_interval = 60 / measStat.max_measurements;
    log_i("Integer divisor timing: %d measurements, transmissions every %d minutes",
          measStat.max_measurements, transmission_interval);
  }
  else
  {
    log_i("Non-divisor timing: %d measurements, transmissions when count reached",
          measStat.max_measurements);
  }

  /*!< Reset measurement count */
  measStat.measurement_count = 0;
  measStat.avg_measurements = 0;

  sysData.sent_ok = false;

  memset(sysData.Date, 0, sizeof(sysData.Date));
  memset(sysData.Time, 0, sizeof(sysData.Time));

  measStat.measurement_count = 0; /*!< Number of measurements in the current cycle */
  measStat.curr_minutes = 0;
  measStat.curr_seconds = 0;
  measStat.curr_total_seconds = 0;

  // STEP 4: Request network connection and wait for NTP sync
  log_i("=== STEP 4: Requesting network connection and NTP sync ===");
  if (sysStat.configuration == true || sysStat.server_ok == true)
  {
    log_i("Configuration available, requesting network connection...");
    // Request network connection (time sync will happen automatically)
    requestNetworkConnection();
    mainStateMachine.current_state = SYS_STATE_WAIT_FOR_NTP_SYNC; // Wait for NTP sync first
    mainStateMachine.next_state = SYS_STATE_WAIT_FOR_NTP_SYNC;
  }
  else
  {
    log_e("Device has no valid configuration, going into error mode!");
    mainStateMachine.current_state = SYS_STATE_ERROR; // set initial state to error if no config
    mainStateMachine.next_state = SYS_STATE_ERROR;    // set next state
  }

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  mainStateMachine.isFirstTransition = 1; // set first transition flag

  // Network task already created earlier in setup

} // end of SETUP
//*******************************************************************************************************************************
//*******************************************************************************************************************************
//*******************************************************************************************************************************

//*******************************************************************************************************************************
//********************************************  L O O P  ************************************************************************
//*******************************************************************************************************************************
void loop()
{
  switch (mainStateMachine.current_state) // state machine for the main loop
  {
  case SYS_STATE_WAIT_FOR_NTP_SYNC:
  {
    log_i("Waiting for NTP synchronization...");
    vMsp_updateDataAndEvent(DISP_EVENT_WAIT_FOR_NETWORK_CONN);
    tTaskDisplay_sendEvent(&displayData);

    // Wait for network connection and automatic time sync from network task
    if (pdTRUE == waitForNetworkEvent(NET_EVENT_TIME_SYNCED, pdMS_TO_TICKS(5000)))
    {
      log_i("NTP sync completed, starting measurement cycle");
      mainStateMachine.next_state = SYS_STATE_WAIT_FOR_TIMEOUT;
    }
    else
    {
      // Keep waiting - network task is still working on connection and sync
      log_v("Still waiting for NTP sync...");
      mainStateMachine.next_state = SYS_STATE_WAIT_FOR_NTP_SYNC;
    }
    break;
  }

  case SYS_STATE_UPDATE_DATE_TIME:
  {
    vMsp_updateDataAndEvent(DISP_EVENT_WAIT_FOR_NETWORK_CONN);
    tTaskDisplay_sendEvent(&displayData);

    // Wait for network connection and automatic time sync from network task
    if (pdTRUE == waitForNetworkEvent(NET_EVENT_TIME_SYNCED, pdMS_TO_TICKS(90000)))
    {
      log_i("Network connected and time synchronized successfully");
      mainStateMachine.next_state = SYS_STATE_WAIT_FOR_TIMEOUT;
    }
    else
    {
      vMsp_updateDataAndEvent(DISP_EVENT_NETWORK_CONN_FAIL);
      tTaskDisplay_sendEvent(&displayData);
      log_e("Failed to connect to the network for date and time update!");
      mainStateMachine.next_state = SYS_STATE_ERROR;
    }
    break;
  }

  case SYS_STATE_WAIT_FOR_TIMEOUT:
  {
    if (getLocalTime(&timeinfo))
    {
      measStat.curr_minutes = timeinfo.tm_min;
      measStat.curr_seconds = timeinfo.tm_sec;
      measStat.curr_total_seconds = measStat.curr_minutes * SEC_IN_MIN + measStat.curr_seconds;

      // Calculate the timeout in seconds taking into account the additional delay for PMS5003 preheating
      measStat.timeout_seconds = ((measStat.curr_total_seconds + measStat.additional_delay) % measStat.delay_between_measurements);

      // kick the pms up when the timeout count starts
      if ((measStat.isPmsAwake == false) && (sensorData.status.PMS5003Sensor))
      {
        log_i("Starting PMS sensor");
        measStat.isPmsAwake = true;
        pms.wakeUp();
      }

      // It is time for a measurement
      if ((measStat.timeout_seconds == 0) && (measStat.curr_minutes != 0))
      {
        log_i("Timeout expired!");
        log_i("Current time: %02d:%02d:%02d", timeinfo.tm_hour, measStat.curr_minutes, measStat.curr_seconds);
        mainStateMachine.next_state = SYS_STATE_READ_SENSORS; // go to read sensors state
      }
      else
      {
        // if it is not the first transition, wait for timeout
        if ((measStat.isSensorDataAvailable == false) && (mainStateMachine.isFirstTransition))
        {
          vMsp_updateDataAndEvent(DISP_EVENT_WAIT_FOR_TIMEOUT);
          tTaskDisplay_sendEvent(&displayData);
          delay(500);
        }
      }
    }
    else
    {
      log_e("Failed to obtain time from NTP!");
      mainStateMachine.next_state = SYS_STATE_WAIT_FOR_TIMEOUT;
    }
    break;
  }

  case SYS_STATE_READ_SENSORS:
  {
    log_i("Reading sensors...");

    if (mainStateMachine.isFirstTransition)
    {
      mainStateMachine.isFirstTransition = 0; // reset first transition flag
      // Reset sensor variables
      sensorData.gasData.temperature = 0.0f;
      sensorData.gasData.pressure = 0.0f;
      sensorData.gasData.humidity = 0.0f;
      sensorData.gasData.volatileOrganicCompounds = 0.0f;
      sensorData.airQualityData.particleMicron1 = 0.0f;
      sensorData.airQualityData.particleMicron25 = 0.0f;
      sensorData.airQualityData.particleMicron10 = 0.0f;
      sensorData.pollutionData.data.carbonMonoxide = 0.0f;
      sensorData.pollutionData.data.nitrogenDioxide = 0.0f;
      sensorData.pollutionData.data.ammonia = 0.0f;
      sensorData.ozoneData.ozone = 0.0f;

      err.BMEfails = 0;
      err.PMSfails = 0;
      err.MICSfails = 0;
      err.O3fails = 0;
      // Reset error counters
      err.count = 0;

      // Update the number of measurements before the next data transmission
      measStat.avg_measurements = measStat.max_measurements - (measStat.curr_minutes % measStat.max_measurements); // Calculate the number of measurements to be made in the current cycle

      if (measStat.avg_measurements == 0)
      {
        measStat.avg_measurements = 1; // If the number of measurements is 0, set it to 1
        // it means we are starting the measurement in the exact minute in which we want to send the data
      }

      log_i("First Transition!\n");
      log_i("Current minutes: %d, avg_measurements: %d, max_measurements: %d\n", measStat.curr_minutes, measStat.avg_measurements, measStat.max_measurements);

      // Reset also the measurement count
      measStat.measurement_count = 0;
    }

    vMsp_updateDataAndEvent(DISP_EVENT_READING_SENSORS);
    tTaskDisplay_sendEvent(&displayData);

    // READING BME680
    if (sensorData.status.BME680Sensor)
    {
      log_i("Sampling BME680 sensor...");
      err.count = 0;
      // Attempt to read BME680 sensor with maximum 3 retries
      for (int retry = 0; retry < 3; retry++)
      {
        if (!tHalSensor_checkBMESensor(&bme680))
        {
          err.count++;
          log_w("BME680 sensor check failed, attempt %d/3", retry + 1);
          if (retry == 2) // Last attempt failed
          {
            log_e("Error while sampling BME680 sensor after 3 attempts!");
            err.BMEfails++;
            break;
          }
          delay(1000);
          continue;
        }
        
        if (!bme680.run())
        {
          log_v("BME680 sensor not ready, waiting...");
          delay(100);
          continue;
        }

        // Successfully read sensor data
        localData.temperature = bme680.temperature;
        log_v("Temperature(*C): %.3f", localData.temperature);
        sensorData.gasData.temperature += localData.temperature;

        localData.pressure = bme680.pressure / PERCENT_DIVISOR;
        localData.pressure = (localData.pressure *
                              pow(1 - (STD_TEMP_LAPSE_RATE * sensorData.gasData.seaLevelAltitude /
                                       (localData.temperature + STD_TEMP_LAPSE_RATE * sensorData.gasData.seaLevelAltitude + CELIUS_TO_KELVIN)),
                                  ISA_DERIVED_EXPONENTIAL));
        log_v("Pressure(hPa): %.3f", localData.pressure);
        sensorData.gasData.pressure += localData.pressure;

        localData.humidity = bme680.humidity;
        log_v("Humidity(perc.): %.3f", localData.humidity);
        sensorData.gasData.humidity += localData.humidity;

        localData.volatileOrganicCompounds = bme680.gasResistance / MICROGRAMS_PER_GRAM;
        localData.volatileOrganicCompounds = fHalSensor_no2AndVocCompensation(localData.volatileOrganicCompounds, &localData, &sensorData);
        log_v("Compensated gas resistance(kOhm): %.3f\n", localData.volatileOrganicCompounds);
        sensorData.gasData.volatileOrganicCompounds += localData.volatileOrganicCompounds;
        break;
      }
    }

    // READING MICS6814
    if (sensorData.status.MICS6814Sensor)
    {
      log_i("Sampling MICS6814 sensor...");
      err.count = 0;
      // Attempt to read MICS6814 sensor with maximum 3 retries
      for (int retry = 0; retry < 3; retry++)
      {
        MICS6814SensorReading_t micsLocData;

        micsLocData.carbonMonoxide = gas.measureCO();
        micsLocData.nitrogenDioxide = gas.measureNO2();
        micsLocData.ammonia = gas.measureNH3();

        if ((micsLocData.carbonMonoxide < 0) || (micsLocData.nitrogenDioxide < 0) || (micsLocData.ammonia < 0))
        {
          err.count++;
          log_w("MICS6814 sensor reading failed, attempt %d/3", retry + 1);
          if (retry == 2) // Last attempt failed
          {
            log_e("Error while sampling MICS6814 sensor after 3 attempts!");
            err.MICSfails++;
            break;
          }
          delay(1000);
          continue;
        }

        // Successfully read sensor data
        micsLocData.carbonMonoxide = vGeneric_convertPpmToUgM3(micsLocData.carbonMonoxide, sensorData.pollutionData.molarMass.carbonMonoxide);
        log_v("CO(ug/m3): %.3f", micsLocData.carbonMonoxide);
        sensorData.pollutionData.data.carbonMonoxide += micsLocData.carbonMonoxide;

        micsLocData.nitrogenDioxide = vGeneric_convertPpmToUgM3(micsLocData.nitrogenDioxide, sensorData.pollutionData.molarMass.nitrogenDioxide);
        if (sensorData.status.BME680Sensor)
        {
          micsLocData.nitrogenDioxide = fHalSensor_no2AndVocCompensation(micsLocData.nitrogenDioxide, &localData, &sensorData);
        }
        log_v("NOx(ug/m3): %.3f", micsLocData.nitrogenDioxide);
        sensorData.pollutionData.data.nitrogenDioxide += micsLocData.nitrogenDioxide;

        micsLocData.ammonia = vGeneric_convertPpmToUgM3(micsLocData.ammonia, sensorData.pollutionData.molarMass.ammonia);
        log_v("NH3(ug/m3): %.3f\n", micsLocData.ammonia);
        sensorData.pollutionData.data.ammonia += micsLocData.ammonia;

        break;
      }
    }

    // READING O3
    if (sensorData.status.O3Sensor)
    {
      ze25Data_t o3Data;
      log_i("Sampling O3 sensor...");
      err.count = 0;
      // Attempt to read O3 sensor with maximum 3 retries
      for (int retry = 0; retry < 3; retry++)
      {
        if (!tHalSensor_isAnalogO3Connected())
        {
          err.count++;
          log_w("O3 sensor connection check failed, attempt %d/3", retry + 1);
          if (retry == 2) // Last attempt failed
          {
            log_e("Error while sampling O3 sensor after 3 attempts!");
            err.O3fails++;
            break;
          }
          delay(1000);
          continue;
        }
        
        // Successfully read sensor data
        o3Data.ozone = fHalSensor_analogUgM3O3Read(&localData.temperature, &sensorData);
        log_v("O3(ug/m3): %.3f", o3Data.ozone);
        sensorData.ozoneData.ozone += o3Data.ozone;

        break;
      }
      log_v("O3 sensor reading completed");
    }

    // READING PMS5003
    if (sensorData.status.PMS5003Sensor)
    {
      log_i("Sampling PMS5003 sensor...");
      err.count = 0;
      // Attempt to read PMS5003 sensor with maximum 3 retries
      for (int retry = 0; retry < 3; retry++)
      {
        // Clear serial buffer
        while (pmsSerial.available())
        {
          pmsSerial.read();
        }
        
        if (!pms.readUntil(data))
        {
          err.count++;
          log_w("PMS5003 sensor reading failed, attempt %d/3", retry + 1);
          if (retry == 2) // Last attempt failed
          {
            log_e("Error while sampling PMS5003 sensor after 3 attempts!");
            err.PMSfails++;
            break;
          }
          delay(1000);
          continue;
        }
        
        // Successfully read sensor data
        log_v("PM1(ug/m3): %d", data.PM_AE_UG_1_0);
        sensorData.airQualityData.particleMicron1 += data.PM_AE_UG_1_0;

        log_v("PM2,5(ug/m3): %d", data.PM_AE_UG_2_5);
        sensorData.airQualityData.particleMicron25 += data.PM_AE_UG_2_5;

        log_v("PM10(ug/m3): %d\n", data.PM_AE_UG_10_0);
        sensorData.airQualityData.particleMicron10 += data.PM_AE_UG_10_0;

        break;
      }
    }

    measStat.measurement_count++;
    log_i("Measurement #%d completed", measStat.measurement_count);

    log_i("Sensor values AFTER evaluation:\n");
    log_i("temp: %.2f, hum: %.2f, pre: %.2f, VOC: %.2f, PM1: %d, PM25: %d, PM10: %d, MICS_CO: %.2f, MICS_NO2: %.2f, MICS_NH3: %.2f, ozone: %.2f, MSP: %d, measurement_count: %d\n",
          sensorData.gasData.temperature, sensorData.gasData.humidity, sensorData.gasData.pressure, sensorData.gasData.volatileOrganicCompounds,
          sensorData.airQualityData.particleMicron1, sensorData.airQualityData.particleMicron25, sensorData.airQualityData.particleMicron10,
          sensorData.pollutionData.data.carbonMonoxide, sensorData.pollutionData.data.nitrogenDioxide, sensorData.pollutionData.data.ammonia,
          sensorData.ozoneData.ozone, sensorData.MSP, measStat.measurement_count);
    log_i("Measurements done, going to timeout...\n");
    mainStateMachine.next_state = SYS_STATE_EVAL_SENSOR_STATUS;
    break;
  }

  case SYS_STATE_EVAL_SENSOR_STATUS:
  {
    log_i("Evaluating sensor status...");

    // Update current time for clock-aligned mode evaluation
    if (getLocalTime(&timeinfo))
    {
      measStat.curr_minutes = timeinfo.tm_min;
      log_v("Current time for evaluation: %02d:%02d", timeinfo.tm_hour, measStat.curr_minutes);
    }
    int8_t sens_stat_count = 0;
    for (sens_stat_count = 0; sens_stat_count < SENS_STAT_MAX; sens_stat_count++)
    {
      if (err.senserrs[sens_stat_count] == true)
      {
        switch (sens_stat_count)
        {
        case SENS_STAT_BME680:
          sensorData.status.BME680Sensor = true;
          break;
        case SENS_STAT_PMS5003:
          sensorData.status.PMS5003Sensor = true;
          break;
        case SENS_STAT_MICS6814:
          sensorData.status.MICS6814Sensor = true;
          break;
        case SENS_STAT_O3:
          sensorData.status.O3Sensor = true;
          break;
        }
      }
    }

    // Check if it's time to send data
    if (measStat.measurement_count >= measStat.avg_measurements)
    {
      log_i("All measurements obtained, going to send data...\n");
      mainStateMachine.next_state = SYS_STATE_SEND_DATA; // go to send data state
    }
    else
    {
      // MEASUREMENTS MESSAGE
      log_i("Measurements in progress...\n");
      vMsp_updateDataAndEvent(DISP_EVENT_MEAS_IN_PROGRESS);
      tTaskDisplay_sendEvent(&displayData);

      log_i("Not enough measurements obtained, going to wait for timeout...\n");
      mainStateMachine.next_state = SYS_STATE_WAIT_FOR_TIMEOUT;
    }

    break;
  }

  case SYS_STATE_SEND_DATA:
  {
    log_i("Sending data to server...\n");

    // We have obtained all the measurements, do the mean and transmit the data
    log_i("All measurements obtained, sending data...\n");
    log_i("Sensor values BEFORE AVERAGE:\n");
    log_i("temp: %.2f, hum: %.2f, pre: %.2f, VOC: %.2f, PM1: %d, PM25: %d, PM10: %d, MICS_CO: %.2f, MICS_NO2: %.2f, MICS_NH3: %.2f, ozone: %.2f, MSP: %d, measurement_count: %d vs %d\n",
          sensorData.gasData.temperature, sensorData.gasData.humidity, sensorData.gasData.pressure,
          sensorData.gasData.volatileOrganicCompounds,
          sensorData.airQualityData.particleMicron1, sensorData.airQualityData.particleMicron25, sensorData.airQualityData.particleMicron10,
          sensorData.pollutionData.data.carbonMonoxide, sensorData.pollutionData.data.nitrogenDioxide, sensorData.pollutionData.data.ammonia,
          sensorData.ozoneData.ozone, sensorData.MSP, measStat.measurement_count, measStat.avg_measurements);

    vMsp_updateDataAndEvent(DISP_EVENT_SENDING_MEAS);
    tTaskDisplay_sendEvent(&displayData);

    if (sensorData.status.BME680Sensor || sensorData.status.PMS5003Sensor || sensorData.status.MICS6814Sensor || sensorData.status.MICS4514Sensor || sensorData.status.O3Sensor)
    {
      log_i("Computing averages from %d measurements", measStat.measurement_count);
      vHalSensor_performAverages(&err, &sensorData, &measStat);
    }

    // MSP# Index evaluation
    sensorData.MSP = sHalSensor_evaluateMSPIndex(&sensorData);

    log_i("Sending data to server...");
    send_data_t sendData;
    memcpy(&sendData.sendTimeInfo, &timeinfo, sizeof(tm)); // copy time info to sendData
    log_i("Current time: %02d:%02d:%02d\n", sendData.sendTimeInfo.tm_hour, sendData.sendTimeInfo.tm_min, sendData.sendTimeInfo.tm_sec);
    // The measuremente starts the minute before the timeout, so we need to add the additional delay
    // additional delay in seconds and the next minute
    sendData.sendTimeInfo.tm_sec = 0;
    // Scale up the minutes and hour to obtain the correct time
    if (sendData.sendTimeInfo.tm_min == 59)
    {
      sendData.sendTimeInfo.tm_min = 0;
      sendData.sendTimeInfo.tm_hour++;
      if (sendData.sendTimeInfo.tm_hour >= 24)
      {
        sendData.sendTimeInfo.tm_hour = 0; // reset hour to 0 if it is 24
      }
    }
    else
    {
      sendData.sendTimeInfo.tm_min++;
    }

    log_i("Scaled time: %02d:%02d:%02d\n", sendData.sendTimeInfo.tm_hour, sendData.sendTimeInfo.tm_min, sendData.sendTimeInfo.tm_sec);
    sendData.temp = sensorData.gasData.temperature;
    sendData.hum = sensorData.gasData.humidity;
    sendData.pre = sensorData.gasData.pressure;
    sendData.VOC = sensorData.gasData.volatileOrganicCompounds;
    sendData.PM1 = sensorData.airQualityData.particleMicron1;
    sendData.PM25 = sensorData.airQualityData.particleMicron25;
    sendData.PM10 = sensorData.airQualityData.particleMicron10;
    sendData.MICS_CO = sensorData.pollutionData.data.carbonMonoxide;
    sendData.MICS_NO2 = sensorData.pollutionData.data.nitrogenDioxide;
    sendData.MICS_NH3 = sensorData.pollutionData.data.ammonia;
    sendData.ozone = sensorData.ozoneData.ozone;
    sendData.MSP = sensorData.MSP;

    log_i("Sensor values AFTER AVERAGE:\n");
    log_i("temp: %.2f, hum: %.2f, pre: %.2f, VOC: %.2f, PM1: %d, PM25: %d, PM10: %d, MICS_CO: %.2f, MICS_NO2: %.2f, MICS_NH3: %.2f, ozone: %.2f, MSP: %d, measurement_count: %d\n",
          sensorData.gasData.temperature, sensorData.gasData.humidity, sensorData.gasData.pressure,
          sensorData.gasData.volatileOrganicCompounds,
          sensorData.airQualityData.particleMicron1, sensorData.airQualityData.particleMicron25, sensorData.airQualityData.particleMicron10,
          sensorData.pollutionData.data.carbonMonoxide, sensorData.pollutionData.data.nitrogenDioxide, sensorData.pollutionData.data.ammonia,
          sensorData.ozoneData.ozone, sensorData.MSP, measStat.measurement_count);

    log_i("Send Time: %02d:%02d:%02d\n", sendData.sendTimeInfo.tm_hour, sendData.sendTimeInfo.tm_min, sendData.sendTimeInfo.tm_sec);

    // Enqueue data for transmission by network task
    if (enqueueSendData(sendData, pdMS_TO_TICKS(1000)))
    {
      log_i("Data enqueued successfully for network transmission");
    }
    else
    {
      log_e("Failed to enqueue data for transmission - queue might be full");
    }

    // show the already captured data
    vMsp_updateDataAndEvent(DISP_EVENT_SHOW_MEAS_DATA);
    tTaskDisplay_sendEvent(&displayData);

    log_i("Current time: %02d:%02d:%02d", sendData.sendTimeInfo.tm_hour, sendData.sendTimeInfo.tm_min, sendData.sendTimeInfo.tm_sec);
    log_i("temp: %.2f, hum: %.2f, pre: %.2f, VOC: %.2f, PM1: %d, PM25: %d, PM10: %d, MICS_CO: %.2f, MICS_NO2: %.2f, MICS_NH3: %.2f, ozone: %.2f, MSP: %d, measurement_count: %d vs %d",
          sensorData.gasData.temperature, sensorData.gasData.humidity, sensorData.gasData.pressure,
          sensorData.gasData.volatileOrganicCompounds,
          sensorData.airQualityData.particleMicron1, sensorData.airQualityData.particleMicron25, sensorData.airQualityData.particleMicron10,
          sensorData.pollutionData.data.carbonMonoxide, sensorData.pollutionData.data.nitrogenDioxide, sensorData.pollutionData.data.ammonia,
          sensorData.ozoneData.ozone, sensorData.MSP, measStat.measurement_count, measStat.avg_measurements);

    mainStateMachine.isFirstTransition = 1; // set first transition flag
    // This will clean up the sensor variables for the next cycle
    // set the flag to true, so instead of timeout , the display is shown with previous calculated data.
    measStat.isSensorDataAvailable = true;

    // Reset measurement count for next cycle
    log_i("Transmission complete, resetting measurement count from %d to 0", measStat.measurement_count);
    measStat.measurement_count = 0;

    // Set up for next cycle - the mode will be determined when isFirstTransition is processed
    log_i("Next cycle: will collect up to %d measurements", measStat.max_measurements);

    mainStateMachine.next_state = SYS_STATE_WAIT_FOR_TIMEOUT; // go to wait for timeout state
    break;
  }

  case SYS_STATE_ERROR:
  {
    vMsp_updateDataAndEvent(DISP_EVENT_SYSTEM_ERROR);
    tTaskDisplay_sendEvent(&displayData);
    log_e("System in error state! Waiting for reset...");
    vTaskDelay(portMAX_DELAY);
    break;
  }

  default:
  {
    log_e("Unknown state %d!", mainStateMachine.current_state);
    mainStateMachine.next_state = SYS_STATE_WAIT_FOR_TIMEOUT; // go to error state
    break;
  }
  }
  mainStateMachine.current_state = mainStateMachine.next_state; // update current state to next state
}

/*********************************************************
 * @brief init function
 *
 * @param p_tData
 *********************************************************/
void vMspInit_sensorStatusAndData(sensorData_t *p_tData)
{
  vMspOs_takeDataAccessMutex();

  // -- set default sensor status to disabled
  p_tData->status.BME680Sensor = DISABLED;
  p_tData->status.PMS5003Sensor = DISABLED;
  p_tData->status.MICS6814Sensor = DISABLED;
  p_tData->status.MICS4514Sensor = DISABLED;
  p_tData->status.O3Sensor = DISABLED;

  // -- set gas sensor default values
  p_tData->gasData.humidity = 0.0;
  p_tData->gasData.temperature = 0.0;
  p_tData->gasData.pressure = 0.0;
  p_tData->gasData.volatileOrganicCompounds = 0.0;
  p_tData->gasData.seaLevelAltitude = SEA_LEVEL_ALTITUDE_IN_M;

  // -- set air quality sensor default values
  p_tData->airQualityData.particleMicron1 = 0;
  p_tData->airQualityData.particleMicron25 = 0;
  p_tData->airQualityData.particleMicron10 = 0;

  // -- default R0 values for the sensor (RED, OX, NH3)
  p_tData->pollutionData.sensingResInAir.redSensor = R0_RED_SENSOR;
  p_tData->pollutionData.sensingResInAir.oxSensor = R0_OX_SENSOR;
  p_tData->pollutionData.sensingResInAir.nh3Sensor = R0_NH3_SENSOR;

  // -- default point offset values for sensor measurements (RED, OX, NH3)
  p_tData->pollutionData.sensingResInAirOffset.redSensor = 0;
  p_tData->pollutionData.sensingResInAirOffset.oxSensor = 0;
  p_tData->pollutionData.sensingResInAirOffset.nh3Sensor = 0;

  // -- molar mass of (CO, NO2, NH3)
  p_tData->pollutionData.molarMass.carbonMonoxide = CO_MOLAR_MASS;
  p_tData->pollutionData.molarMass.nitrogenDioxide = NO2_MOLAR_MASS;
  p_tData->pollutionData.molarMass.ammonia = NH3_MOLAR_MASS;

  // -- MICS6814 sensor data
  p_tData->pollutionData.data.carbonMonoxide = 0;
  p_tData->pollutionData.data.nitrogenDioxide = 0;
  p_tData->pollutionData.data.ammonia = 0;

  // -- ozone detection module data
  p_tData->ozoneData.ozone = 0;
  p_tData->ozoneData.o3ZeroOffset = O3_SENS_DISABLE_ZERO_OFFSET;

  // -- compensation parameters for MICS6814-OX and BME680-VOC data
  p_tData->compParams.currentHumidity = HUMIDITY_COMP_PARAM; /*!<default humidity compensation */
  p_tData->compParams.currentTemperature = TEMP_COMP_PARAM;  /*!<default temperature compensation */
  p_tData->compParams.currentPressure = PRESS_COMP_PARAM;    /*!<default pressure compensation */

  p_tData->MSP = MSP_DEFAULT_DATA; /*!<set to -1 to distinguish from grey (0) */

  vMspOs_giveDataAccessMutex();
}

void vMspInit_NetworkAndMeasInfo(void)
{
  // Initialize measurement info
  vMspInit_MeasInfo();

  // Initialize network configuration (network task will load from SD)
  devinfo = deviceNetworkInfo_t{}; // Initialize all members to default values
  devinfo.wifipow = WIFI_POWER_17dBm;
}

void vMspInit_MeasInfo(void)
{
  memset(&measStat, 0, sizeof(deviceMeasurement_t));
  measStat.avg_measurements = 1;
  measStat.isPmsAwake = false;
  measStat.isSensorDataAvailable = false;
}

/**
 * @brief Configure system with SD card data or apply defaults
 * @param sysData System data structure
 * @param sysStat System status structure
 * @param devInfo Device network info structure
 * @param measStat Measurement statistics structure
 */
void vMspInit_configureSystemFromSD(systemData_t *sysData, systemStatus_t *sysStat,
                                    deviceNetworkInfo_t *devInfo, deviceMeasurement_t *measStat)
{
  log_i("Configuring system from SD data or defaults...");

  // Apply configuration defaults if SD card data is missing or invalid
  if (!sysStat->configuration || !sysStat->sdCard)
  {
    log_w("SD configuration unavailable, applying defaults");

    // Network defaults
    if (devInfo->ssid.length() == 0)
    {
      devInfo->ssid = "DefaultSSID";
      log_i("Applied default WiFi SSID");
    }
    if (devInfo->passw.length() == 0)
    {
      devInfo->passw = "DefaultPass";
      log_i("Applied default WiFi password");
    }
    if (devInfo->apn.length() == 0)
    {
      devInfo->apn = "internet";
      log_i("Applied default APN");
    }

    // Server configuration
    if (sysData->server.length() == 0)
    {
#ifdef API_SERVER
      sysData->server = API_SERVER;
      sysStat->server_ok = true;
      log_i("Applied compile-time API server: %s", sysData->server.c_str());
#else
      sysData->server = "milanosmartpark.info";
      sysStat->server_ok = true;
      log_w("Applied fallback server: %s", sysData->server.c_str());
#endif
    }

    // API credentials
    if (sysData->api_secret_salt.length() == 0)
    {
#ifdef API_SECRET_SALT
      sysData->api_secret_salt = API_SECRET_SALT;
      log_i("Applied compile-time API secret");
#else
      sysData->api_secret_salt = "default_salt";
      log_w("Applied fallback API secret");
#endif
    }

    // Measurement configuration
    if (measStat->avg_measurements <= 0)
    {
      measStat->avg_measurements = 5; // Default to 5 measurements
      log_i("Applied default avg_measurements: %d", measStat->avg_measurements);
    }

    // NTP configuration
    if (sysData->ntp_server.length() == 0)
    {
      sysData->ntp_server = NTP_SERVER_DEFAULT;
      log_i("Applied default NTP server: %s", sysData->ntp_server.c_str());
    }
    if (sysData->timezone.length() == 0)
    {
      sysData->timezone = TZ_DEFAULT;
      log_i("Applied default timezone: %s", sysData->timezone.c_str());
    }
  }
  else
  {
    log_i("Using SD card configuration");

    // Validate loaded configuration
    if (measStat->avg_measurements <= 0 || measStat->avg_measurements > 60)
    {
      log_w("Invalid avg_measurements (%d), setting to 5", measStat->avg_measurements);
      measStat->avg_measurements = 5;
    }

    // Ensure server_ok flag is set if server is configured
    if (sysData->server.length() > 0 && !sysStat->server_ok)
    {
      sysStat->server_ok = true;
      log_i("Server configured, setting server_ok flag");
    }
  }

// Apply firmware version
#ifdef VERSION_STRING
  sysData->ver = VERSION_STRING;
#else
  sysData->ver = "DEV";
#endif

  log_i("System configuration completed successfully");
}

void vMspInit_setDefaultNtpTimezoneStatus(systemData_t *p_tData)
{
  // set default values
  p_tData->ntp_server = NTP_SERVER_DEFAULT; /*!<default NTP server */
  p_tData->timezone = TZ_DEFAULT;           /*!<default timezone */
}

void vMsp_updateDataAndEvent(displayEvents_t event)
{
  displayData.currentEvent = event;

  vMspOs_takeDataAccessMutex();

  if (event == DISP_EVENT_SHOW_MEAS_DATA)
  {
    displayData.sensorData = sensorData;
  }
  displayData.devInfo = devinfo;
  displayData.measStat = measStat;
  displayData.sysData = sysData;
  displayData.sysStat = sysStat;

  vMspOs_giveDataAccessMutex();

  // Update the system status queue for the connectivity task
}

void vMsp_setGpioPins(void)
{
  // SET PIN MODES ++++++++++++++++++++++++++++++++++++
  pinMode(33, OUTPUT);
  pinMode(26, OUTPUT);
  pinMode(25, OUTPUT);
  pinMode(O3_ADC_PIN, INPUT_PULLDOWN);
  analogSetAttenuation(ADC_11db);
  pinMode(MODEM_RST, OUTPUT);
  digitalWrite(MODEM_RST, HIGH);
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
}

//************************************** EOF **************************************