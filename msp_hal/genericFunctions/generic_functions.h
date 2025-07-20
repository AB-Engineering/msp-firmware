/**
 * @file generic_functions.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-07-12
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#ifndef GENERIC_FUNCTIONS_H
#define GENERIC_FUNCTIONS_H



/*************************************************
 * @brief   converts float values in strings,
 *          with the decimal part separated 
 *          from the integer part by a comma
 * 
 * @param   value 
 * @return  String 
 *************************************************/
static String dspFloatToComma(float value);

/*************************************************
 * @brief   converts float values in strings 
 *          with the decimal part separated 
 *          from the integer part by a comma
 * 
 * @param   value 
 * @return  String 
 ************************************************/
String floatToComma(float value);


/************************************************************
 * @brief calculates ug/m3 from a gas ppm concentration
 * 
 * @param ppm 
 * @param mm 
 * @return float 
 ***********************************************************/
float convertPpmToUgM3(float ppm, float mm);


#endif