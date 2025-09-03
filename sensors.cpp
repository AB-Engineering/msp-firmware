/****************************************************************************
 * @file    sensors.cpp
 * @author  Refactored by AB-Engineering - https://ab-engineering.it
 * @brief   functions to fetch the data from sensors and process
 * @version 0.1
 * @date    2025-07-26
 *
 * @copyright Copyright (c) 2025
 *
 ***************************************************************************/

// -- includes
#include <stdint.h>
#include <stdio.h>
#include "config.h"
#include "generic_functions.h"
#include <MiCS6814-I2C.h>
#include "sensors.h"
#include <stdbool.h>

// PM25 THRESHOLDS
#define PM25_HIGH_LEVEL 50
#define PM25_MID_LEVEL 25
#define PM25_LOW_LEVEL 10
// NO THRESHOLDS
#define NO_HIGH_LEVEL 400
#define NO_MID_LEVEL 200
#define NO_LOW_LEVEL 100
// O3 THRESHOLDS
#define O3_HIGH_LEVEL 240
#define O3_MID_LEVEL 180
#define O3_LOW_LEVEL 120

/***************************************************
 * @brief checks BME680 status
 *
 * @return true
 * @return false
 ***************************************************/
mspStatus_t tHalSensor_checkBMESensor(Bsec *ptr)
{
  if (ptr->bsecStatus < BSEC_OK)
  {
    log_e("BSEC error, status %d!", ptr->bsecStatus);
    return STATUS_ERR;
  }
  else if (ptr->bsecStatus > BSEC_OK)
  {
    log_w("BSEC warning, status %d!", ptr->bsecStatus);
  }

  if (ptr->bme68xStatus < BME68X_OK)
  {
    log_e("Sensor error, bme680_status %d!", ptr->bme68xStatus);
    return STATUS_ERR;
  }
  else if (ptr->bme68xStatus > BME68X_OK)
  {
    log_w("Sensor warning, status %d!", ptr->bme68xStatus);
  }

  ptr->bsecStatus = BSEC_OK;
  return STATUS_OK;
}

/***************************************************
 * @brief checks analog ozone sensor status
 *
 * @return mspStatus_t
 ***************************************************/
mspStatus_t tHalSensor_isAnalogO3Connected()
{
  u_int16_t detect = 0;
  detect = analogRead(O3_ADC_PIN);
  pinMode(O3_ADC_PIN, INPUT_PULLDOWN); // must invoke after every analogRead
  log_d("Detected points: %d", detect);
  if (detect == STATUS_ERR)
  {
    return STATUS_ERR;
  }
  return STATUS_OK;
}

/********************************************************************
 * @brief write firmware calibration values into MICS6814's EEPROM
 *        Store new base resistance values in EEPROM
 *
 * @param p_tData
 ********************************************************************/
void vHalSensor_writeMicsValues(sensorData_t *p_tData)
{
  Wire.beginTransmission(DATA_I2C_ADDR);
  Wire.write(CMD_V2_SET_R0);
  Wire.write(p_tData->pollutionData.sensingResInAir.nh3Sensor >> 8); // NH3
  Wire.write(p_tData->pollutionData.sensingResInAir.nh3Sensor & 0xFF);
  Wire.write(p_tData->pollutionData.sensingResInAir.redSensor >> 8); // RED
  Wire.write(p_tData->pollutionData.sensingResInAir.redSensor & 0xFF);
  Wire.write(p_tData->pollutionData.sensingResInAir.oxSensor >> 8); // OX
  Wire.write(p_tData->pollutionData.sensingResInAir.oxSensor & 0xFF);
  Wire.endTransmission();
}

/**********************************************************
 * @brief   check if MICS6814 internal values are
 *          the same as firmware defaults
 *
 * @param p_tData
 * @return mspStatus_t
 **********************************************************/
mspStatus_t tHalSensor_checkMicsValues(sensorData_t *p_tData, sensorR0Value_t *ptr)
{
  if ((ptr->redSensor == p_tData->pollutionData.sensingResInAir.redSensor) && (ptr->oxSensor == p_tData->pollutionData.sensingResInAir.oxSensor) && (ptr->nh3Sensor == p_tData->pollutionData.sensingResInAir.nh3Sensor))
  {
    return STATUS_OK;
  }
  return STATUS_ERR;
}

/*********************************************************************************
 * @brief compensate gas sensor readings (specifically for NO₂ and VOCs)
 *        based on environmental conditions: temperature, pressure, and humidity.
 *        for NO2 and VOC gas compensations
 *
 * @param inputGas
 * @param p_tcurrData
 * @param p_tData
 * @return float
 **********************************************************************************/
float fHalSensor_no2AndVocCompensation(float inputGas, bme680Data_t *p_tcurrData, sensorData_t *p_tData)
{
  return (inputGas * (((p_tcurrData->humidity + HUMIDITY_OFFSET) / PERCENT_DIVISOR) * p_tData->compParams.currentHumidity)) +
         ((p_tcurrData->temperature - REFERENCE_TEMP_C) * p_tData->compParams.currentTemperature) -
         ((p_tcurrData->pressure - REFERENCE_PRESSURE_HPA) * p_tData->compParams.currentPressure);
}

/********************************************************************************
 * @brief reads and calculates ozone ppm value from analog ozone sensor
 *
 * @param intemp
 * @param p_tData
 * @return float
 *******************************************************************************/
float fHalSensor_analogUgM3O3Read(float *intemp, sensorData_t *p_tData)
{

  int points = 0;
  float currTemp = REFERENCE_TEMP_C; // initialized at OSHA standard conditions for temperature compensation
  if (p_tData->status.BME680Sensor)
  {
    currTemp = *intemp; // using current measured temperature
    log_d("Current measured temperature is %.3f", currTemp);
  }
  const short readtimes = 10; // reading 10 times for good measure
  for (short i = 0; i < readtimes; i++)
  {
    int readnow = analogRead(O3_ADC_PIN);
    pinMode(O3_ADC_PIN, INPUT_PULLDOWN); // must invoke after every analogRead
    log_v("ADC Read is: %d", readnow);
    points += readnow;
    delay(10);
  }
  points /= readtimes;
  log_d("ADC Read averaged is: %d", points);
  points -= p_tData->ozoneData.o3ZeroOffset;
  if (points <= 0)
    return 0.0;
  return (((points * O3_CALC_FACTOR_1) * O3_CALC_FACTOR_2 * O3_CALC_FACTOR_3) / (CELIUS_TO_KELVIN + currTemp)); // temperature compensated
}

/******************************************************************************
 * @brief   print measurements to serial output
 *
 * @param data
 *****************************************************************************/
void vHalSensor_printMeasurementsOnSerial(send_data_t *data, sensorData_t *p_tPtr)
{

  char locDate[DATE_LEN] = {0};
  char locTime[TIME_LEN] = {0};

  strftime(locDate, sizeof(locDate), "%d/%m/%Y", &data->sendTimeInfo); // Formatting date as DD/MM/YYYY
  strftime(locTime, sizeof(locDate), "%T", &data->sendTimeInfo);       // Formatting time as HH:MM:SS

  log_i("Measurements log:"); // Log measurements to serial output
  log_i("Date&time: %s %s", locDate, locTime);
  if (p_tPtr->status.BME680Sensor)
  {
    log_i("Temperature: %.2f°C", data->temp);
    log_i("Humidity: %.2f%%", data->hum);
    log_i("Pressure: %.2f hPa", data->pre);
    log_i("VOC: %.2f kOhm", data->VOC);
  }
  if (p_tPtr->status.PMS5003Sensor)
  {
    log_i("PM10: %d ug/m3", data->PM10);
    log_i("PM2.5: %d ug/m3", data->PM25);
    log_i("PM1: %d ug/m3", data->PM1);
  }
  if (p_tPtr->status.O3Sensor)
  {
    log_i("O3: %.2f ug/m3", data->ozone);
  }
  if (p_tPtr->status.MICS6814Sensor)
  {
    log_i("NOx: %.2f ug/m3", data->MICS_NO2);
    log_i("CO: %.2f ug/m3", data->MICS_CO);
    log_i("NH3: %.2f ug/m3", data->MICS_NH3);
  }
  log_i("Measurements logged successfully");
}

/*****************************************************************************************************
 * @brief
 *
 * @param p_tErr
 * @param p_tData
 * @param number_of_measurements
 *****************************************************************************************************/
void vHalSensor_performAverages(errorVars_t *p_tErr, sensorData_t *p_tData, deviceMeasurement_t *p_tMeas)
{

  log_i("=== AVERAGING CALCULATION ===");
  log_i("Total measurement_count: %d", p_tMeas->measurement_count);
  log_i("Error counts: BME=%d, PMS=%d, MICS=%d, O3=%d", p_tErr->BMEfails, p_tErr->PMSfails, p_tErr->MICSfails, p_tErr->O3fails);

  short runs = p_tMeas->measurement_count - p_tErr->BMEfails;
  log_i("BME680: runs = %d - %d = %d", p_tMeas->measurement_count, p_tErr->BMEfails, runs);
  if (p_tData->status.BME680Sensor && runs > 0)
  {
    log_i("BME680 BEFORE: temp=%.3f, pressure=%.3f, humidity=%.3f", 
          p_tData->gasData.temperature, p_tData->gasData.pressure, p_tData->gasData.humidity);
    p_tData->gasData.temperature /= runs;
    p_tData->gasData.pressure /= runs;
    p_tData->gasData.humidity /= runs;
    p_tData->gasData.volatileOrganicCompounds /= runs;
    log_i("BME680 AFTER: temp=%.3f, pressure=%.3f, humidity=%.3f (divided by %d)", 
          p_tData->gasData.temperature, p_tData->gasData.pressure, p_tData->gasData.humidity, runs);
  }
  else if (p_tData->status.BME680Sensor)
  {
    p_tData->status.BME680Sensor = false;
    p_tErr->senserrs[SENS_STAT_BME680] = true;
  }

  runs = p_tMeas->measurement_count - p_tErr->PMSfails;
  if (p_tData->status.PMS5003Sensor && runs > 0)
  {
    float pmValue = 0.0f;

    pmValue = p_tData->airQualityData.particleMicron1 / runs;

    if ((pmValue - (int32_t) pmValue) >= ROUNDING_THRESHOLD)
    {
      p_tData->airQualityData.particleMicron1 = (int32_t) pmValue + 1;
    }
    else
    {
      p_tData->airQualityData.particleMicron1 = (int32_t) pmValue;
    }

    pmValue = p_tData->airQualityData.particleMicron25 / runs;

    if ((pmValue - (int32_t) pmValue) >= ROUNDING_THRESHOLD)
    {
      p_tData->airQualityData.particleMicron25 = (int32_t) pmValue + 1;
    }
    else
    {
      p_tData->airQualityData.particleMicron25 = (int32_t) pmValue;
    }

    pmValue = p_tData->airQualityData.particleMicron10 / runs;
    if ((pmValue - (int32_t) pmValue) >= ROUNDING_THRESHOLD)
    {
      p_tData->airQualityData.particleMicron10 = (int32_t) pmValue + 1;
    }
    else
    {
      p_tData->airQualityData.particleMicron10 = (int32_t) pmValue;
    }
  }
  else if (p_tData->status.PMS5003Sensor)
  {
    p_tData->status.PMS5003Sensor = false;
    p_tErr->senserrs[SENS_STAT_PMS5003] = true;
  }

  runs = p_tMeas->measurement_count - p_tErr->MICSfails;
  if (p_tData->status.MICS6814Sensor && runs > 0)
  {
    p_tData->pollutionData.data.carbonMonoxide /= runs;
    p_tData->pollutionData.data.nitrogenDioxide /= runs;
    p_tData->pollutionData.data.ammonia /= runs;
  }
  else if (p_tData->status.MICS6814Sensor)
  {
    p_tData->status.MICS6814Sensor = false;
    p_tErr->senserrs[SENS_STAT_MICS6814] = true;
  }

  runs = p_tMeas->measurement_count - p_tErr->O3fails;
  if (p_tData->status.O3Sensor && runs > 0)
  {
    p_tData->ozoneData.ozone /= runs;
  }
  else if (p_tData->status.O3Sensor)
  {
    p_tData->status.O3Sensor = false;
    p_tErr->senserrs[SENS_STAT_O3] = true;
  }
}

/*****************************************************************************************************
 * @brief   evaluates the MSP# index from ug/m3 concentrations of specific gases using standard
 *          IAQ values (needs 1h averages)
 *
 *          possible returned values are:
 *          0 -> n.d.(grey);
 *          1 -> good(green);
 *          2 -> acceptable(yellow);
 *          3 -> bad(red);
 *          4 -> really bad(black)
 *
 * @param  p_tData  pointer to sensor data
 * @return short
 *******************************************************************************************************/
short sHalSensor_evaluateMSPIndex(sensorData_t *p_tData)
{
  log_i("Evaluating MSP# index...\n");

  short msp[MSP_INDEX_MAX] = {0, 0, 0}; // msp[0] is for pm2.5, msp[1] is for nox, msp[2] is for o3

  if (p_tData->status.PMS5003Sensor)
  {
    if (p_tData->airQualityData.particleMicron25 > PM25_HIGH_LEVEL)
      msp[MSP_INDEX_PM25] = 4;
    else if (p_tData->airQualityData.particleMicron25 > PM25_MID_LEVEL)
      msp[MSP_INDEX_PM25] = 3;
    else if (p_tData->airQualityData.particleMicron25 > PM25_LOW_LEVEL)
      msp[MSP_INDEX_PM25] = 2;
    else
      msp[MSP_INDEX_PM25] = 1;
  }
  if (p_tData->status.MICS6814Sensor)
  {
    if (p_tData->pollutionData.data.nitrogenDioxide > NO_HIGH_LEVEL)
      msp[MSP_INDEX_NO2] = 4;
    else if (p_tData->pollutionData.data.nitrogenDioxide > NO_MID_LEVEL)
      msp[MSP_INDEX_NO2] = 3;
    else if (p_tData->pollutionData.data.nitrogenDioxide > NO_LOW_LEVEL)
      msp[MSP_INDEX_NO2] = 2;
    else
      msp[MSP_INDEX_NO2] = 1;
  }
  if (p_tData->status.O3Sensor)
  {
    if (p_tData->ozoneData.ozone > O3_HIGH_LEVEL)
      msp[MSP_INDEX_O3] = 4;
    else if (p_tData->ozoneData.ozone > O3_MID_LEVEL)
      msp[MSP_INDEX_O3] = 3;
    else if (p_tData->ozoneData.ozone > O3_LOW_LEVEL)
      msp[MSP_INDEX_O3] = 2;
    else
      msp[MSP_INDEX_O3] = 1;
  }

  if ((msp[MSP_INDEX_PM25] > 0) && (msp[MSP_INDEX_NO2] > 0) && (msp[MSP_INDEX_O3] > 0) && ((msp[MSP_INDEX_PM25] == msp[MSP_INDEX_NO2]) || (msp[MSP_INDEX_PM25] == msp[MSP_INDEX_O3]) || (msp[MSP_INDEX_NO2] == msp[MSP_INDEX_O3])))
  { // return the most dominant
    if (msp[MSP_INDEX_NO2] == msp[MSP_INDEX_O3])
      return msp[MSP_INDEX_NO2];
    else
      return msp[MSP_INDEX_PM25];
  }
  else
  { // return the worst one
    if ((msp[MSP_INDEX_PM25] > msp[MSP_INDEX_NO2]) && (msp[MSP_INDEX_PM25] > msp[MSP_INDEX_O3]))
      return msp[MSP_INDEX_PM25];
    else if (msp[MSP_INDEX_NO2] > msp[MSP_INDEX_O3])
      return msp[MSP_INDEX_NO2];
    else
      return msp[MSP_INDEX_O3];
  }
}