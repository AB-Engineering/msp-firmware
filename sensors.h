/******************************************************************************
 * @file    sensors.h
 * @author  Refactored by AB-Engineering - https://ab-engineering.it
 * @brief   Milano Smart Park Firmware
            Developed by Norman Mulinacci
            This code is usable under the terms and conditions of the
            GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
 * @version 0.1
 * @date    2025-07-26
 * 
 * @copyright Copyright (c) 2025
 * 
 *****************************************************************************/

#ifndef SENSORS_H
#define SENSORS_H

// -- includes --
#include "shared_values.h"
#include <bsec.h>


/***************************************************
 * @brief checks BME680 status
 *
 * @param ptr Pointer to Bsec object
 * @return true
 * @return false
 ***************************************************/
mspStatus_t tHalSensor_checkBMESensor(Bsec *ptr);

/***************************************************
 * @brief checks analog ozone sensor status
 *
 * @return mspStatus_t
 ***************************************************/
mspStatus_t tHalSensor_isAnalogO3Connected();

/********************************************************************
 * @brief write firmware calibration values into MICS6814's EEPROM
 *        Store new base resistance values in EEPROM
 *
 * @param p_tData
 ********************************************************************/
void vHalSensor_writeMicsValues(sensorData_t *p_tData);

/**********************************************************
 * @brief   check if MICS6814 internal values are
 *          the same as firmware defaults
 *
 * @param p_tData
 * @param ptr
 * @return mspStatus_t
 **********************************************************/
mspStatus_t tHalSensor_checkMicsValues(sensorData_t *p_tData, sensorR0Value_t *ptr);

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
float fHalSensor_no2AndVocCompensation(float inputGas, bme680Data_t *p_tcurrData, sensorData_t *p_tData);

/********************************************************************************
 * @brief reads and calculates ozone ppm value from analog ozone sensor
 *
 * @param intemp
 * @param p_tData 
 * @return float
 *******************************************************************************/
float fHalSensor_analogUgM3O3Read(float *intemp, sensorData_t *p_tData);

/*******************************************************************************
 * @brief print measurements to serial output
 * 
 * @param data 
 * @param p_tPtr 
 *******************************************************************************/
void vHalSensor_printMeasurementsOnSerial(send_data_t *data, sensorData_t *p_tPtr);


/*****************************************************************************************************
 * @brief 
 * 
 * @param p_tErr 
 * @param p_tData 
 * @param number_of_measurements 
 *****************************************************************************************************/
void vHalSensor_performAverages(errorVars_t *p_tErr, sensorData_t *p_tData, deviceMeasurement_t *p_tMeas);


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
short sHalSensor_evaluateMSPIndex(sensorData_t *p_tData);


#endif