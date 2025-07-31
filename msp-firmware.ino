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

#define API_SECRET_SALT "2198be9d8e83a662210ef9cd8acc5b36"
#define API_SERVER      "milanosmartpark.info"

// Hardware UART definitions. Modes: UART0=0(debug out); UART1=1; UART2=2

HardwareSerial pmsSerial(2);

// Pin to get semi-random data from for SSL
// Pick a pin that's not connected or attached to a randomish voltage source
const int rand_pin = 35;

// Initialize the SSL client library
// We input a client, our trust anchors, and the analog pin
WiFiClient wifi_base;

SSLClient wificlient(wifi_base, TAs, (size_t)TAs_NUM, rand_pin, 1, SSLClient::SSL_ERROR);


// ------------------------------- DEFINES -----------------------------------------------------------------------------

#define TIME_MIN_TRIG 0 /*!<Time in minutes to trigger a data update to the server */
#define TIME_SEC_TRIG 0 /*!<Time in seconds to trigger a data update to the server */

#define MIN_TO_SEC(x) ((x) * 60)    /*!<Macro to convert minutes to seconds */
#define HOUR_TO_SEC(x) ((x) * 3600) /*!<Macro to convert hours to seconds */

#define SEC_IN_HOUR 3600 /*!<Seconds in an hour */
#define SEC_IN_MIN 60    /*!<Seconds in a minute */

#define SEND_DATA_TIMEOUT_IN_SEC (5 * SEC_IN_MIN) /*!<Timeout for sending data to server in seconds, defaults to 5 minutes */

#define PMS_PREHEAT_TIME_IN_SEC 20 /*!<PMS5003 preheat time in seconds, defaults to 45 seconds */

#define EMPTY_STR   ""
// ------------------------------- INSTANCES  --------------------------------------------------------------------------

// -- BME680 sensor instance 
Bsec bme680;

// -- PMS5003 sensor instance 
PMS pms(pmsSerial);

// -- MICS6814 sensors instances
MiCS6814 gas;

// -- Create a new state machine instance
state_machine_t mainStateMachine;

// -- sensor data and status instance
sensorData_t sensorData;

// -- bme 680 data instance  
bme680Data_t localData;

// -- reading status instance 
errorVars_t err;

// -- system status instance
systemStatus_t sysStat;

// -- device information instance
deviceNetworkInfo_t devinfo;

// -- device measurement status
deviceMeasurement_t measStat;

// -- time information data
tm timeinfo;

// -- time zone and ntp server data
systemData_t sysData;

// -- structure for PMS5003
PMS::DATA data;

displayData_t displayData;

send_data_t dataToSend;

//---------------------------------------- FUNCTIONS ----------------------------------------------------------------------

void vMspInit_sensorStatusAndData(sensorData_t *p_tData);

void vMspInit_setDefaultNtpTimezoneStatus(systemData_t *p_tData);

void vMspInit_setDefaultSslName(systemData_t *p_tData);

void vMspInit_setApiSecSaltAndFwVer(systemData_t *p_tData);

void vMsp_updateDataAndEvent(displayEvents_t event);

void vMsp_setGpioPins(void);

void vMspInit_NetworkAndMeasInfo(void);
//*******************************************************************************************************************************
//******************************************  SERVER CONNECTION TASK  ***********************************************************
//*******************************************************************************************************************************



/******************************************************************************************************************/

// Task stack size and priority
#define SEND_DATA_TASK_STACK_SIZE (8 * 1024) // 8KB stack size for the task
#define SEND_DATA_TASK_PRIORITY 1

// Static task variables
StaticTask_t sendDataTaskBuffer;
StackType_t sendDataTaskStack[SEND_DATA_TASK_STACK_SIZE];
TaskHandle_t sendDataTaskHandle = NULL;

void sendDataTask(void *pvParameters);

#define SEND_DATA_QUEUE_LENGTH 4

QueueHandle_t sendDataQueue = NULL;

// Wrapper to send data to the queue
bool enqueueSendData(const send_data_t &data, TickType_t ticksToWait = portMAX_DELAY)
{
  if (sendDataQueue == NULL)
    return false;
  return xQueueSend(sendDataQueue, &data, ticksToWait) == pdPASS;
}

// Wrapper to receive data from the queue
bool dequeueSendData(send_data_t *data, TickType_t ticksToWait = portMAX_DELAY)
{
  if (sendDataQueue == NULL || data == NULL)
    return false;
  return xQueueReceive(sendDataQueue, data, ticksToWait) == pdPASS;
}

// Create the queue before using it (call this in setup)
void initSendDataQueue()
{
  if (sendDataQueue == NULL)
  {
    sendDataQueue = xQueueCreate(SEND_DATA_QUEUE_LENGTH, sizeof(send_data_t));
  }
}

EventGroupHandle_t networkEventGroup = NULL;
StaticEventGroup_t networkEventGroupBuffer; /*!< Static buffer for the event group */

// Create a series of events to handle the network status connection.
void createNetworkEvents()
{
  networkEventGroup = xEventGroupCreateStatic(&networkEventGroupBuffer);
  if (networkEventGroup == NULL)
  {
    log_e("Failed to create network event group!");
  }
  else
  {
    log_i("Network event group created successfully.");
  }
}

bool sendNetworkEvent(net_evt_t event)
{
  if (networkEventGroup == NULL)
  {
    log_e("Network event group not initialized!");
    return false;
  }

  xEventGroupSetBits(networkEventGroup, event);

  return true;
}

bool waitForNetworkEvent(net_evt_t event, TickType_t ticksToWait)
{
  if (networkEventGroup == NULL)
  {
    log_e("Network event group not initialized!");
    return false;
  }

  bool result = true;

  if (xEventGroupWaitBits(networkEventGroup, event, pdTRUE, pdTRUE, ticksToWait) != pdTRUE)
  {
    log_e("Failed to wait for network event: %d", event);
    result = false;
  }
  return result;
}




//*******************************************************************************************************************************

//*******************************************************************************************************************************
//******************************************  S E T U P  ************************************************************************
//*******************************************************************************************************************************
void setup()
{
  memset(&mainStateMachine, 0, sizeof(state_machine_t)); // Initialize the state machine structure

  vMsp_setGpioPins();

  vMspOs_initDataAccessMutex(); // Create the OS mutex for data access

  // init the sensor data structure /status / offset values with defualt values 
  vMspInit_sensorStatusAndData(&sensorData);

  // init device network parameters
  vMspInit_NetworkAndMeasInfo();

  // init the ntp server and timezone data with default values
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

  // Initialize the data queue:
  initSendDataQueue();

  // Create the network event group
  createNetworkEvents();

  // BOOT STRINGS ++++++++++++++++++++++++++++++++++++++++++++++++++++++
  Serial.println("\nMILANO SMART PARK");
  Serial.println("FIRMWARE " + sysData.ver + " by Norman Mulinacci");
  Serial.println("Refactor and optimization by AB-Engineering - https://ab-engineering.it");
  Serial.println("Compiled " + String(__DATE__) + " " + String(__TIME__) + "\n");

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
  

  vHalNetwork_printWiFiMACAddr(&sysStat,&devinfo);

  log_i("printed mac address...\n");
  
  vHalSdcard_readSD(&sysStat,&devinfo,&sensorData,&measStat,&sysData);

  measStat.max_measurements = measStat.avg_measurements; // set current measurements to average measurements

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

    if (tHalSensor_checkMicsValues(&sensorData,&r0Values) == STATUS_OK)
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

  // Calculate delay between measurements
  measStat.delay_between_measurements = SEC_IN_MIN;

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

  // Initialize the send data task
  sendDataTaskHandle = xTaskCreateStaticPinnedToCore(
      sendDataTask,              // Task function
      "SendDataTask",            // Name
      SEND_DATA_TASK_STACK_SIZE, // Stack size
      NULL,                      // Parameters
      SEND_DATA_TASK_PRIORITY,   // Priority
      sendDataTaskStack,         // Stack buffer
      &sendDataTaskBuffer,       // Task buffer
      1                          // Core 1
  );

  // CONNECT TO INTERNET AND GET DATE&TIME +++++++++++++++++++++++++++++++++++++++++++++++++++
  if (sysStat.configuration == true)
  {
    log_i("Wait for network connection...\n");
    mainStateMachine.current_state = SYS_STATE_UPDATE_DATE_TIME; // set initial state
    mainStateMachine.next_state = SYS_STATE_UPDATE_DATE_TIME;    // set next state
  }
  else
  {
    mainStateMachine.current_state = SYS_STATE_ERROR; // set initial state
    mainStateMachine.next_state = SYS_STATE_ERROR;    // set next state
  }

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  mainStateMachine.isFirstTransition = 1; // set first transition flag

} // end of SETUP
//*******************************************************************************************************************************
//*******************************************************************************************************************************
//*******************************************************************************************************************************

#define NTP_SYNC_TX_COUNT 100
// sendData task function
void sendDataTask(void *pvParameters)
{
  log_i("Send Data Task started on core %d", xPortGetCoreID());
  memset(&dataToSend, 0, sizeof(send_data_t)); // Initialize the data structure
  uint8_t isConnectionInited = false;
  int8_t ntpSyncExpired = NTP_SYNC_TX_COUNT;
  while (1)
  {
    if (isConnectionInited == false)
    {
      if (sysStat.configuration)
      {
        log_i("Initializing connection to the network...\n");
        vHalNetwork_connAndGetTime(&sysStat,&devinfo,&sysData,&timeinfo);

        if (sysStat.connection)
        {
          sendNetworkEvent(NET_EVENT_CONNECTED); // Notify that we are connected
          isConnectionInited = true;             // Set the connection as initialized
          ntpSyncExpired = NTP_SYNC_TX_COUNT;    // Reset the NTP sync counter

          // If using the modem, disconnect to save GB
          if (sysStat.use_modem)
          {
            // Disconnect the modem after sending data
            if (vHalNetwork_modemDisconnect() == true)
              sysStat.connection = false;
          }
        }
      }
    }
    else
    {
      if (dequeueSendData(&dataToSend, portMAX_DELAY))
      {
        if (sysStat.connection == false)
        {
          ntpSyncExpired--;
          // We are not connected to the internet, try a new connection
          if (sysStat.configuration)
          {
            if (ntpSyncExpired <= 0)
            {
              log_i("NTP sync expired, resync...\n");
              ntpSyncExpired = NTP_SYNC_TX_COUNT; // Reset the NTP sync counter
              vHalNetwork_connAndGetTime(&sysStat,&devinfo,&sysData,&timeinfo);
            }
            else
            {
              log_i("Connect without NTP sync");
              vHalNetwork_connectToInternet(&sysStat,&devinfo);
            }

            if (sysStat.connection)
            {
              sendNetworkEvent(NET_EVENT_CONNECTED); // Notify that we are connected
            }
          }
        }

        // Connect and send data to the server
        if (sysStat.server && sysStat.connection && sysStat.datetime)
        {
          SSLClient* client = tHalNetwork_getGSMClient();
          client = (sysStat.use_modem) ? client : &wificlient;
          vHalNetwork_connectToServer(client,&dataToSend,&devinfo,&sensorData,&sysData);
        }

        Serial.println("Writing log to SD card...");
        if (sysStat.sdCard)
        {
          if (uHalSdcard_checkLogFile(&devinfo))
          {
            vHalSdcard_logToSD(&dataToSend,&sysData,&sysStat,&sensorData,&devinfo);
          }
        }

        // Print measurements to serial and draw them on the display
        vHalSensor_printMeasurementsOnSerial(&dataToSend, &sensorData);

        if (sysStat.use_modem)
        {
          // Disconnect the modem after sending data
          if (vHalNetwork_modemDisconnect() == true)
            sysStat.connection = false;
        }
      }
    }
  }
}

//*******************************************************************************************************************************
//********************************************  L O O P  ************************************************************************
//*******************************************************************************************************************************
void loop()
{
  switch (mainStateMachine.current_state)  // state machine for the main loop
  {
    case SYS_STATE_UPDATE_DATE_TIME:
    {
      vMsp_updateDataAndEvent(DISP_EVENT_WAIT_FOR_NETWORK_CONN);
      tTaskDisplay_sendEvent(&displayData);
      if (pdTRUE == waitForNetworkEvent(NET_EVENT_CONNECTED, portMAX_DELAY))
      {
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
      if ((measStat.isPmsWokenUp == false) && (sensorData.status.PMS5003Sensor))
      {
        Serial.println("lets start the PMS sensor ");
        measStat.isPmsWokenUp = true;
        pms.wakeUp();
      }

      // It is time for a measurement
      if ((measStat.timeout_seconds == 0) && (measStat.curr_minutes != 0))
      {
        Serial.println("Timeout expired!");
        Serial.printf("Current time: %02d:%02d:%02d\n", timeinfo.tm_hour, measStat.curr_minutes, measStat.curr_seconds);
        mainStateMachine.next_state = SYS_STATE_READ_SENSORS; // go to read sensors state
      }
      else
      {
        // if it is not the first transition, wait for timeout
        if ((measStat.isSensorDataAvailable == false) && (mainStateMachine.isFirstTransition))
        {
          vMsp_updateDataAndEvent(DISP_EVENT_WAIT_FOR_TIMEOUT);
          tTaskDisplay_sendEvent(&displayData);
          delay(1000);
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
    Serial.println("Reading sensors...");

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
      while (1)
      { // TODO: add a timeout to this loop
        if (!tHalSensor_checkBMESensor(&bme680))
        {
          if (err.count > 2)
          {
            log_e("Error while sampling BME680 sensor!");
            err.BMEfails++;
            break;
          }
          delay(1000);
          err.count++;
          continue;
        }
        if (!bme680.run())
        {
          continue;
        }

        localData.temperature = bme680.temperature;
        log_v("Temperature(*C): %.3f", localData.temperature);
        sensorData.gasData.temperature += localData.temperature;

        localData.pressure = bme680.pressure / PERCENT_DIVISOR;
        localData.pressure = (localData.pressure * 
          pow(1 - (STD_TEMP_LAPSE_RATE * sensorData.gasData.seaLevelAltitude / 
            (localData.temperature + STD_TEMP_LAPSE_RATE *
               sensorData.gasData.seaLevelAltitude + CELIUS_TO_KELVIN)), ISA_DERIVED_EXPONENTIAL));
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
      while (1)
      { // TODO: add a timeout to this loop
        MICS6814SensorReading_t micsLocData;
        
        micsLocData.carbonMonoxide = gas.measureCO();
        micsLocData.nitrogenDioxide = gas.measureNO2();
        micsLocData.ammonia = gas.measureNH3();

        if ((micsLocData.carbonMonoxide < 0) || (micsLocData.nitrogenDioxide < 0) || (micsLocData.ammonia < 0))
        {
          if (err.count > 2)
          {
            log_e("Error while sampling!");
            err.MICSfails++;
            break;
          }
          err.count++;
          delay(1000);
          continue;
        }

        micsLocData.carbonMonoxide = vGeneric_convertPpmToUgM3(micsLocData.carbonMonoxide,sensorData.pollutionData.molarMass.carbonMonoxide);
        log_v("CO(ug/m3): %.3f", micsLocData.carbonMonoxide);
        sensorData.pollutionData.data.carbonMonoxide += micsLocData.carbonMonoxide;

        micsLocData.nitrogenDioxide = vGeneric_convertPpmToUgM3(micsLocData.nitrogenDioxide,sensorData.pollutionData.molarMass.nitrogenDioxide);
        if (sensorData.status.BME680Sensor)
        {
          micsLocData.nitrogenDioxide = fHalSensor_no2AndVocCompensation(micsLocData.nitrogenDioxide,&localData,&sensorData);
        }
        log_v("NOx(ug/m3): %.3f", micsLocData.nitrogenDioxide);
        sensorData.pollutionData.data.nitrogenDioxide += micsLocData.nitrogenDioxide;

        micsLocData.ammonia = vGeneric_convertPpmToUgM3(micsLocData.ammonia,sensorData.pollutionData.molarMass.ammonia);
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
      while (1)
      { // TODO: add a timeout to this loop
        if (!tHalSensor_isAnalogO3Connected())
        {
          if (err.count > 2)
          {
            log_e("Error while sampling O3 sensor!");
            err.O3fails++;
            break;
          }
          delay(1000);
          err.count++;
          continue;
        }
        o3Data.ozone = fHalSensor_analogUgM3O3Read(&localData.temperature,&sensorData);
        log_v("O3(ug/m3): %.3f", o3Data.ozone);
        sensorData.ozoneData.ozone += o3Data.ozone;

        break;
      }
      Serial.println();
    }

    // READING PMS5003
    if (sensorData.status.PMS5003Sensor)
    {
      log_i("Sampling PMS5003 sensor...");
      err.count = 0;
      while (1)
      { // TODO: add a timeout to this loop
        while (pmsSerial.available())
        {
          pmsSerial.read();
        }
        if (!pms.readUntil(data))
        {
          if (err.count > 2)
          {
            log_e("Error while sampling PMS5003 sensor!");
            err.PMSfails++;
            break;
          }
          delay(1000);
          err.count++;
          continue;
        }
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

    log_i("Sensor values AFTER evaluation:\n");
    log_i("temp: %.2f, hum: %.2f, pre: %.2f, VOC: %.2f, PM1: %d, PM25: %d, PM10: %d, MICS_CO: %.2f, MICS_NO2: %.2f, MICS_NH3: %.2f, ozone: %.2f, MSP: %d, measurement_count: %d\n",
          sensorData.gasData.temperature, sensorData.gasData.humidity, sensorData.gasData.pressure, sensorData.gasData.volatileOrganicCompounds,
          sensorData.airQualityData.particleMicron1,sensorData.airQualityData.particleMicron25,sensorData.airQualityData.particleMicron10,
          sensorData.pollutionData.data.carbonMonoxide, sensorData.pollutionData.data.nitrogenDioxide, sensorData.pollutionData.data.ammonia,
          sensorData.ozoneData.ozone,sensorData.MSP, measStat.measurement_count);
    log_i("Measurements done, going to timeout...\n");
    mainStateMachine.next_state = SYS_STATE_EVAL_SENSOR_STATUS;
    break;
  }

  case SYS_STATE_EVAL_SENSOR_STATUS:
  {
    Serial.println("Evaluating sensor status...");
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
      vHalSensor_performAverages(&err,&sensorData,&measStat);
    }

    // MSP# Index evaluation
    sensorData.MSP = sHalSensor_evaluateMSPIndex(&sensorData);

    Serial.println("Sending data to server...");
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

    enqueueSendData(sendData);

    //show the already captured data
    vMsp_updateDataAndEvent(DISP_EVENT_SHOW_MEAS_DATA);
    tTaskDisplay_sendEvent(&displayData);

    Serial.printf("Current time: %02d:%02d:%02d\n", sendData.sendTimeInfo.tm_hour, sendData.sendTimeInfo.tm_min, sendData.sendTimeInfo.tm_sec);
    Serial.printf("temp: %.2f, hum: %.2f, pre: %.2f, VOC: %.2f, PM1: %d, PM25: %d, PM10: %d, MICS_CO: %.2f, MICS_NO2: %.2f, MICS_NH3: %.2f, ozone: %.2f, MSP: %d, measurement_count: %d vs %d\n",
    sensorData.gasData.temperature, sensorData.gasData.humidity, sensorData.gasData.pressure,
    sensorData.gasData.volatileOrganicCompounds, 
    sensorData.airQualityData.particleMicron1, sensorData.airQualityData.particleMicron25, sensorData.airQualityData.particleMicron10,
    sensorData.pollutionData.data.carbonMonoxide, sensorData.pollutionData.data.nitrogenDioxide, sensorData.pollutionData.data.ammonia,
    sensorData.ozoneData.ozone, sensorData.MSP, measStat.measurement_count, measStat.avg_measurements);

    mainStateMachine.isFirstTransition = 1; // set first transition flag
    // This will clean up the sensor variables for the next cycle
    // set the flag to true, so instead of timeout , the display is shown with previous calculated data. 
    measStat.isSensorDataAvailable == true;

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
  p_tData->gasData.seaLevelAltitude = 122.0; /*!<sea level altitude in meters, defaults to Milan, Italy */

  // -- set air quality sensor default values
  p_tData->airQualityData.particleMicron1 = 0;
  p_tData->airQualityData.particleMicron25 = 0;
  p_tData->airQualityData.particleMicron10 = 0;

  // -- default R0 values for the sensor (RED, OX, NH3)
  p_tData->pollutionData.sensingResInAir.redSensor = 955;
  p_tData->pollutionData.sensingResInAir.oxSensor = 900;
  p_tData->pollutionData.sensingResInAir.nh3Sensor = 163;

  // -- default point offset values for sensor measurements (RED, OX, NH3)
  p_tData->pollutionData.sensingResInAirOffset.redSensor = 0;
  p_tData->pollutionData.sensingResInAirOffset.oxSensor = 0;
  p_tData->pollutionData.sensingResInAirOffset.nh3Sensor = 0;

  // -- molar mass of (CO, NO2, NH3)
  p_tData->pollutionData.molarMass.carbonMonoxide = 28.01f;
  p_tData->pollutionData.molarMass.nitrogenDioxide = 46.01f;
  p_tData->pollutionData.molarMass.ammonia = 17.03f;

  // -- MICS6814 sensor data
  p_tData->pollutionData.data.carbonMonoxide = 0;
  p_tData->pollutionData.data.nitrogenDioxide = 0;
  p_tData->pollutionData.data.ammonia = 0;

  // -- ozone detection module data
  p_tData->ozoneData.ozone = 0;
  p_tData->ozoneData.o3ZeroOffset = -1; /*!< ozone sensor ADC zero default offset is 1489, -1 to disable it (0.4V to 1.1V range in 12bit resolution at 0dB attenuation) */

  // -- compensation parameters for MICS6814-OX and BME680-VOC data
  p_tData->compParams.currentHumidity = 0.6f;   /*!<default humidity compensation */
  p_tData->compParams.currentTemperature = 1.352f;  /*!<default temperature compensation */
  p_tData->compParams.currentTemperature = 0.0132f;   /*!<default pressure compensation */

  p_tData->MSP = -1; /*!<set to -1 to distinguish from grey (0) */

  vMspOs_giveDataAccessMutex();

}

void vMspInit_NetworkAndMeasInfo(void)
{
  devinfo.wifipow = WIFI_POWER_17dBm;
  devinfo.ssid = EMPTY_STR;
  devinfo.passw = EMPTY_STR;
  devinfo.apn = EMPTY_STR;
  devinfo.deviceid = EMPTY_STR;
  devinfo.logpath = EMPTY_STR;

  memset(&measStat,0,sizeof(deviceMeasurement_t));
  measStat.avg_measurements = 1;
  measStat.isPmsWokenUp = false;
  measStat.isSensorDataAvailable = false;

}

void vMspInit_setDefaultNtpTimezoneStatus(systemData_t *p_tData)
{
  // clean the structure 
  memset(p_tData,0,sizeof(systemData_t));

  // set default values
  p_tData->ntp_server = "pool.ntp.org"; /*!<default NTP server */
  p_tData->timezone = "GMT0";           /*!<default timezone */
}

void vMspInit_setDefaultSslName(systemData_t *p_tData)
{
  // Default server name for SSL data upload
#ifdef API_SERVER
  p_tData->server = API_SERVER;
  p_tData->server_ok = true;
#else
  p_tData->server = "";
  p_tData->server_ok = false;
#endif

}

void vMspInit_setApiSecSaltAndFwVer(systemData_t *p_tData)
{
  #ifdef API_SECRET_SALT
  p_tData->api_secret_salt = API_SECRET_SALT;
#else
  p_tData->api_secret_salt = "secret_salt";
#endif

// current firmware version
#ifdef VERSION_STRING
  p_tData->ver = VERSION_STRING;
#else
  p_tData->ver = "DEV";
#endif

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