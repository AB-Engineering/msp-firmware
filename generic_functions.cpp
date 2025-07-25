/**
 * @file generic_functions.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-07-12
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "stdio.h"
#include "math.h"
#include "generic_functions.h"
#include "shared_values.h"
#include <Arduino.h>


/*************************************************
 * @brief   converts float values in strings,
 *          with the decimal part separated 
 *          from the integer part by a comma
 * 
 * @param   value 
 * @return  String 
 *************************************************/
void dspFloatToComma(float value, char *buffer, size_t bufferSize)
{
  int intPart = (int)value;
  int decimalPart = (int) (fabs(value - intPart) * 100);
  snprintf(buffer,bufferSize,"%d,%02d",intPart,decimalPart);
}

/*************************************************
 * @brief   converts float values in strings 
 *          with the decimal part separated 
 *          from the integer part by a comma
 * 
 * @param   value 
 * @return  String 
 ************************************************/
String floatToComma(float value)
{
  String convert = String(value, 3);
  convert.replace(STR_DOT, STR_COMMA);
  return convert;
}


/************************************************************
 * @brief calculates ug/m3 from a gas ppm concentration
 * 
 * @param ppm 
 * @param mm 
 * @return float 
 ***********************************************************/
float convertPpmToUgM3(float ppm, float mm)
{
  float temperatureK = (REFERENCE_TEMP_C) + (CELIUS_TO_KELVIN);

  float molarVolume = (GAS_CONSTANT * temperatureK) / REFERENCE_PRESSURE_HPA;
  
  return (ppm * (MICROGRAMS_PER_GRAM)) * (mm / molarVolume);
}
