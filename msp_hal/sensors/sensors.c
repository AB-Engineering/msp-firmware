/**
 * @file sensors.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-07-12
 * 
 * @copyright Copyright (c) 2025
 * 
 */



// -- includes 
#include <stdint.h>
#include <stdio.h>

#include "generic_functions.h"
#include "sensors.h"

// -- Sensors management libraries
#include <bsec.h>
// for MICS6814
#include <MiCS6814-I2C.h>



#define true     1
#define false    0
/***************************************************
 * @brief checks BME680 status 
 * 
 * @return true 
 * @return false 
 ***************************************************/
mspStatus_t tMspHal_checkBMESensor(Bsec *ptr)
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
mspStatus_t tMspHal_isAnalogO3Connected()
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
void vMspHal_writeMicsValues(sensorData_t *p_tData)
{
  Wire.beginTransmission(DATA_I2C_ADDR);
  Wire.write(CMD_V2_SET_R0);
  Wire.write(p_tData->pollutionData.sensingResInAir.nh3Sensor >> 8);    // NH3  
  Wire.write(p_tData->pollutionData.sensingResInAir.nh3Sensor & 0xFF);
  Wire.write(p_tData->pollutionData.sensingResInAir.redSensor >> 8);    // RED
  Wire.write(p_tData->pollutionData.sensingResInAir.redSensor & 0xFF);
  Wire.write(p_tData->pollutionData.sensingResInAir.oxSensor >> 8);     // OX
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
mspStatus_t tMspHal_checkMicsValues(sensorData_t *p_tData, MiCS6814 *ptr)
{
  sensorR0Value_t r0Values;
  r0Values.redSensor = ptr->getBaseResistance(CH_RED);
  r0Values.oxSensor = ptr->getBaseResistance(CH_OX);
  r0Values.nh3Sensor = ptr->getBaseResistance(CH_NH3);
  if (r0Values.redSensor == p_tData->pollutionData.sensingResInAir.redSensor
    && r0Values.oxSensor  == p_tData->pollutionData.sensingResInAir.oxSensor
    && r0Values.nh3Sensor == p_tData->pollutionData.sensingResInAir.nh3Sensor)
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
float fMspHal_no2AndVocCompensation(float inputGas, bme680Data_t* p_tcurrData, sensorData_t* p_tData)
{
  return (inputGas * (((p_tcurrData->humidity + HUMIDITY_OFFSET) / PERCENT_DIVISOR) * p_tData->compParams.currentHumidity)) + 
        ((p_tcurrData->temperature - REFERENCE_TEMP_C) * p_tData->compParams.currentTemperature) - 
        ((p_tcurrData->pressure - REFERENCE_PRESSURE_HPA) * p_tData->compParams.currentPressure);
}


/********************************************************************************
 * @brief reads and calculates ozone ppm value from analog ozone sensor
 *
 * @param intemp
 * @return float
 *******************************************************************************/
float analogUgM3O3Read(float *intemp, sensorData_t *p_tData)
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
  return (((points * 2.03552924) * 12.187 * 48) / (273.15 + currTemp)); // temperature compensated
}

/******************************************************************************
 * @brief   serial print function 
 * 
 * @param data 
 *****************************************************************************/
void printMeasurementsOnSerial(send_data_t *data, sensorData_t *p_tPtr )
{

  char locDate[11] = {0};
  char locTime[9] = {0};

  strftime(locDate, sizeof(locDate), "%d/%m/%Y", &data->sendTimeInfo); // Formatting date as DD/MM/YYYY
  strftime(locTime, sizeof(locDate), "%T", &data->sendTimeInfo);       // Formatting time as HH:MM:SS

  Serial.println("Measurements log:\n"); // Log measurements to serial output
  Serial.println("Date&time: " + String(locDate) + " " + String(locTime) + "\n");
  if (p_tPtr->status.BME680Sensor)
  {
    Serial.println("Temperature: " + floatToComma(data->temp) + "°C");
    Serial.println("Humidity: " + floatToComma(data->hum) + "%");
    Serial.println("Pressure: " + floatToComma(data->pre) + "hPa");
    Serial.println("VOC: " + floatToComma(data->VOC) + "kOhm");
  }
  if (p_tPtr->status.PMS5003Sensor)
  {
    Serial.println("PM10: " + String(data->PM10) + "ug/m3");
    Serial.println("PM2,5: " + String(data->PM25) + "ug/m3");
    Serial.println("PM1: " + String(data->PM1) + "ug/m3");
  }
  if (p_tPtr->status.O3Sensor)
  {
    Serial.println("O3: " + floatToComma(data->ozone) + "ug/m3");
  }
  if (p_tPtr->status.MICS6814Sensor)
  {
    Serial.println("NOx: " + floatToComma(data->MICS_NO2) + "ug/m3");
    Serial.println("CO: " + floatToComma(data->MICS_CO) + "ug/m3");
    Serial.println("NH3: " + floatToComma(data->MICS_NH3) + "ug/m3");
  }
  Serial.println();
}



/*****************************************************************************************************
 * @brief 
 * 
 * @param p_tErr 
 * @param p_tData 
 * @param number_of_measurements 
 *****************************************************************************************************/
void performAverages(errorVars_t *p_tErr, sensorData_t *p_tData, deviceMeasurement_t *p_tMeas)
{

  log_i("Performing averages...\n");

  short runs = p_tMeas->measurement_count - p_tErr->BMEfails;
  if (p_tData->status.BME680Sensor && runs > 0)
  {
    p_tData->gasData.temperature /= runs;
    p_tData->gasData.pressure /= runs;
    p_tData->gasData.humidity /= runs;
    p_tData->gasData.volatileOrganicCompounds /= runs;
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

    if (pmValue - int(pmValue) >= 0.5f)
    {
      p_tData->airQualityData.particleMicron1 = int(pmValue) + 1;
    }
    else
    {
      p_tData->airQualityData.particleMicron1 = int(pmValue);
    }

    pmValue = p_tData->airQualityData.particleMicron25 / runs;
    
    if (pmValue - int(pmValue) >= 0.5f)
    {
      p_tData->airQualityData.particleMicron25 = int(pmValue) + 1;
    }
    else
    {
      p_tData->airQualityData.particleMicron25 = int(pmValue);
    }

    pmValue = p_tData->airQualityData.particleMicron10 / runs;
    if (pmValue - int(pmValue) >= 0.5f)
    {
      p_tData->airQualityData.particleMicron10 = int(pmValue) + 1;
    }
    else
    {
      p_tData->airQualityData.particleMicron10 = int(pmValue);
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
short evaluateMSPIndex(sensorData_t *p_tData)
{
  log_i("Evaluating MSP# index...\n");

  short msp[MSP_INDEX_MAX] = {0, 0, 0}; // msp[0] is for pm2.5, msp[1] is for nox, msp[2] is for o3

  if (p_tData->status.PMS5003Sensor)
  {
    if (p_tData->airQualityData.particleMicron25 > 50)
      msp[MSP_INDEX_PM25] = 4;
    else if (p_tData->airQualityData.particleMicron25 > 25)
      msp[MSP_INDEX_PM25] = 3;
    else if (p_tData->airQualityData.particleMicron25 > 10)
      msp[MSP_INDEX_PM25] = 2;
    else
      msp[MSP_INDEX_PM25] = 1;
  }
  if (p_tData->status.MICS6814Sensor)
  {
    if (p_tData->pollutionData.data.nitrogenDioxide > 400)
      msp[MSP_INDEX_NO2] = 4;
    else if (p_tData->pollutionData.data.nitrogenDioxide > 200)
      msp[MSP_INDEX_NO2] = 3;
    else if (p_tData->pollutionData.data.nitrogenDioxide > 100)
      msp[MSP_INDEX_NO2] = 2;
    else
      msp[MSP_INDEX_NO2] = 1;
  }
  if (p_tData->status.O3Sensor)
  {
    if (p_tData->ozoneData.ozone > 240)
      msp[MSP_INDEX_O3] = 4;
    else if (p_tData->ozoneData.ozone > 180)
      msp[MSP_INDEX_O3] = 3;
    else if (p_tData->ozoneData.ozone > 120)
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