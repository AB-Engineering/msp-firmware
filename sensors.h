/*
                        Milano Smart Park Firmware
                       Developed by Norman Mulinacci

          This code is usable under the terms and conditions of the
             GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
*/

// Sensors and Data Management Functions

#ifndef SENSORS_H
#define SENSORS_H

// -- includes --
#include "shared_values.h"

// -- Sensors management libraries
#include <bsec.h>
// #include "network/network.h"



//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/***************************************************
 * @brief checks BME680 status
 *
 * @return true
 * @return false
 ***************************************************/
mspStatus_t tMspHal_checkBMESensor(Bsec *ptr);

/***************************************************
 * @brief checks analog ozone sensor status
 *
 * @return mspStatus_t
 ***************************************************/
mspStatus_t tMspHal_isAnalogO3Connected();

/********************************************************************
 * @brief write firmware calibration values into MICS6814's EEPROM
 *        Store new base resistance values in EEPROM
 *
 * @param p_tData
 ********************************************************************/
void vMspHal_writeMicsValues(sensorData_t *p_tData);

/**********************************************************
 * @brief   check if MICS6814 internal values are
 *          the same as firmware defaults
 *
 * @param p_tData
 * @return mspStatus_t
 **********************************************************/
mspStatus_t tMspHal_checkMicsValues(sensorData_t *p_tData, sensorR0Value_t *ptr);

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
float fMspHal_no2AndVocCompensation(float inputGas, bme680Data_t *p_tcurrData, sensorData_t *p_tData);

/********************************************************************************
 * @brief reads and calculates ozone ppm value from analog ozone sensor
 *
 * @param intemp
 * @return float
 *******************************************************************************/
float analogUgM3O3Read(float *intemp, sensorData_t *p_tData);


/******************************************************************************
 * @brief   serial print function 
 * 
 * @param data 
 *****************************************************************************/
void printMeasurementsOnSerial(send_data_t *data, sensorData_t *p_tPtr);


/*****************************************************************************************************
 * @brief 
 * 
 * @param p_tErr 
 * @param p_tData 
 * @param number_of_measurements 
 *****************************************************************************************************/
void performAverages(errorVars_t *p_tErr, sensorData_t *p_tData, deviceMeasurement_t *p_tMeas);


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
short evaluateMSPIndex(sensorData_t *p_tData);


#endif