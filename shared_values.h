/****************************************************************************************
 * @file    shared_values.h
 * @author  AB-Engineering - https://ab-engineering.it
 * @brief   Shared values and definitions for the Milano Smart Park project
 * @version 0.1
 * @date    2025-07-26
 * 
 * @copyright Copyright (c) 2025
 * 
 ****************************************************************************************/

#ifndef SHARED_VALUES_H
#define SHARED_VALUES_H

// -- includes --
#include <Arduino.h>
#include <WiFiGeneric.h>
#include <stdbool.h>

//-- pin defines --
// O3 sensor ADC pin
#define O3_ADC_PIN 32

// --local defines--
#define STR_DOT                 "."
#define STR_COMMA               ","

#define DATE_LEN                    11
#define TIME_LEN                    9

typedef struct __STATE_MACHINE__ {
  uint8_t current_state;
  uint8_t next_state;
  uint8_t isFirstTransition;
  uint8_t returnState;
} state_machine_t;

typedef enum __SYSTEM_STATES__ {
  SYS_STATE_WAIT_FOR_TIMEOUT,
  SYS_STATE_READ_SENSORS ,
  SYS_STATE_ERROR,
  SYS_STATE_UPDATE_DATE_TIME,
  SYS_STATE_EVAL_SENSOR_STATUS,
  SYS_STATE_SEND_DATA,
  SYS_STATE_MAX_STATES
} system_states_t;

typedef enum __MY_NETWORK_EVENTS__ {
  NET_EVENT_CONNECTED     = (1 << 0), /*!< Network connected event */
  NET_EVENT_DISCONNECTED  = (1 << 1), /*!< Network disconnected event */
  //------------------------------
  NET_EVENT_MAX               /*!< Maximum number of events */
} net_evt_t;

typedef enum __MSP_STAT_ {
  STATUS_ERR,
  STATUS_OK,
}mspStatus_t;


typedef struct __PERIPHERAL_STAT__ {
  uint8_t BME680Sensor;
  uint8_t PMS5003Sensor;
  uint8_t MICS6814Sensor;
  uint8_t MICS4514Sensor;
  uint8_t O3Sensor;
} peripheralStatus_t;

typedef struct __BME680_DATA__ {
  float humidity;
  float temperature;
  float pressure;
  float volatileOrganicCompounds;
  float seaLevelAltitude;
} bme680Data_t;

typedef struct __PMS5003_DATA__ {
  int32_t particleMicron1;  //particles that are 1.0 microns or smaller in diameter
  int32_t particleMicron25; //particles that are 2.5 microns or smaller in diameter
  int32_t particleMicron10; //particles that are 10.0 microns or smaller in diameter
} pms5003Data_t;

typedef struct __MICS6814_RESISTANCE_VAL__ {
  uint16_t redSensor;
  uint16_t oxSensor;
  uint16_t nh3Sensor;
} sensorR0Value_t;

typedef struct __MICS6814_OFFSET_VAL__ {
  int16_t redSensor;  //Typical CO detection
  int16_t oxSensor;   //Typical NO2 detection
  int16_t nh3Sensor;  //Typical NH3 detection
} sensorOffsetValue_t;

typedef struct __MICS6814_MOLAR_MASS_VAL__ {
  float carbonMonoxide;   //red sensor 
  float nitrogenDioxide;  //ox sensor 
  float ammonia;          //NH3 sensor
} sensorMolarMassValue_t;

typedef struct __MICS6814_DATA_ {
  float carbonMonoxide;                       // Carbon monoxide CO 1 – 1000ppm
  float nitrogenDioxide;                      // Nitrogen dioxide NO2 0.05 – 10ppm
  float ammonia;                              // Ammonia NH3 1 – 500ppm
}MICS6814SensorReading_t;

typedef struct __MICS6814_DATA__ {
  sensorR0Value_t sensingResInAir;            // sensingResistanceInAir
  sensorOffsetValue_t sensingResInAirOffset;  // sensor offset values
  sensorMolarMassValue_t molarMass;           // molar mass data
  MICS6814SensorReading_t data;      
}MICS6814Data_t;  

typedef struct __ZE25_O3_VAL__ {
  float ozone;
  int32_t o3ZeroOffset;
} ze25Data_t;

typedef struct __COMPENSATION_PARAMS_ {
  float currentTemperature;
  float currentPressure;
  float currentHumidity;
  float inputGasResistance;
}compensationsParams_t;

typedef struct __SENSOR_DATA__ {
  peripheralStatus_t status;
  bme680Data_t gasData;
  pms5003Data_t airQualityData;
  MICS6814Data_t pollutionData;
  ze25Data_t ozoneData;
  compensationsParams_t compParams;   // Variables for compensation (MICS6814-OX and BME680-VOC)
  int8_t MSP;
}sensorData_t;

typedef enum __SENSOR_STATUS_ARRAY__
{
  SENS_STAT_BME680 = 0,   // BME680 sensor status
  SENS_STAT_PMS5003 = 1,  // PMS5003 sensor status
  SENS_STAT_MICS6814 = 2, // MICS6814 sensor status
  SENS_STAT_O3 = 3,       // Ozone sensor status
  SENS_STAT_MAX = 4
} sens_status_t;

typedef struct __LOOP_VARIABLES_ {
  int32_t count;
  int8_t BMEfails;
  int8_t PMSfails;
  int8_t MICSfails;
  int8_t O3fails;
  bool senserrs[SENS_STAT_MAX];
}errorVars_t;

typedef enum __MSP_ELEMENT_INDEX__
{
  MSP_INDEX_PM25 = 0, // PM2.5 index
  MSP_INDEX_NO2 = 1,  // NO2 index
  MSP_INDEX_O3 = 2,   // O3 index
  MSP_INDEX_MAX = 3
} msp_index_t;

//-- system status --
typedef struct __SYSTEM_STATUS__ {
  uint8_t sdCard;
  uint8_t configuration;
  uint8_t connection;
  uint8_t use_modem;
  uint8_t datetime;
  uint8_t server_ok;
} systemStatus_t;

typedef struct __NETWORK__ {
  String ssid;
  String passw;
  String apn;
  String deviceid;
  String logpath;
  wifi_power_t wifipow;
  char baseMacChr[18] = {0};
  String remain;
  String noNet;
  String foundNet;
}deviceNetworkInfo_t;

typedef struct __MEASUREMENT__ {
  int32_t avg_measurements;
  int32_t avg_delay;
  int32_t max_measurements;
  int32_t measurement_count; /*!< Number of measurements in the current cycle */
  int32_t curr_minutes;
  int32_t curr_seconds;
  int32_t curr_total_seconds;
  int32_t delay_between_measurements; /*!< Delay between measurements in seconds */
  int32_t additional_delay;
  uint32_t timeout_seconds;
  uint8_t isPmsWokenUp;
  uint8_t isSensorDataAvailable;
}deviceMeasurement_t;


typedef struct __SYSTEMDATA_T__ {
  String ntp_server;  // NTP server string
  String timezone;    // time zone string 
  uint8_t sent_ok;    // Sending data to server was successful?
  uint8_t server_ok;  // Default server name for SSL data upload
  String server;
  String api_secret_salt;
  String ver;
  String currentDataTime;
  char Date[DATE_LEN];
  char Time[TIME_LEN];
}systemData_t;

typedef struct __SEND_DATA__
{
  tm sendTimeInfo; /*!< Date and time of the data to be sent */
  float temp;
  float hum;
  float pre;
  float VOC;
  int32_t PM1;
  int32_t PM25;
  int32_t PM10;
  float MICS_CO;
  float MICS_NO2;
  float MICS_NH3;
  float ozone;
  int8_t MSP; /*!< MSP# Index */
} send_data_t;


#endif