/***********************************************************************************************
 * @file    mspOs.cpp
 * @author  AB-Engineering - https://ab-engineering.it
 * @brief   management for the Milano Smart Park project
 * @details This file contains functions to manage mutexes for thread safety in the project.
 * @version 0.1
 * @date    2025-07-25
 *
 * @copyright Copyright (c) 2025
 *
 ***********************************************************************************************/

// -- includes --
#include "mspOs.h"

// -- global mutex handle
SemaphoreHandle_t dataAccessMutex = NULL;


/**************************************************
 * @brief Initializes the mutex for data access.
 *
 **************************************************/
void vMspOs_initDataAccessMutex()
{
    dataAccessMutex = xSemaphoreCreateMutex();
    if (dataAccessMutex == NULL)
    {
        Serial.println("Failed to create mutex!");
    }
}

/**********************************************************
 * @brief   Takes (locks) the mutex for data access.
 *
 *********************************************************/
void vMspOs_takeDataAccessMutex()
{
    if (dataAccessMutex != NULL)
    {
        xSemaphoreTake(dataAccessMutex, portMAX_DELAY); // Wait indefinitely
    }
}

/**********************************************************
 * @brief  Releases (unlocks) the mutex for data access.
 *
 **********************************************************/
void vMspOs_giveDataAccessMutex()
{
    if (dataAccessMutex != NULL)
    {
        xSemaphoreGive(dataAccessMutex);
    }
}