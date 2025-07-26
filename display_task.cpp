/********************************************************************
 * @file display_tasks.cpp
 * @author AB-Engineering - https://ab-engineering.it
 * @brief   Display task for managing display events and rendering
 * @details This task handles various display events, manages the state machine for display updates,
 * @version 0.1
 * @date 2025-07-20
 * 
 * @copyright Copyright (c) 2025
 * 
 *********************************************************************/
// -- Includes --
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "display_task.h"
#include "display.h"

//--------------------------------------------------------------------------------------------------
//------------------ DISPLAY TASK SECTION ----------------------------------------------------------
//--------------------------------------------------------------------------------------------------

// -- defines --
// task stack size and priority
#define DISPLAY_TASK_STACK_SIZE (8 * 1024)
#define DISPLAY_TASK_PRIORITY 1 

#define DISP_QUEUE_LENGTH 			3
#define DISP_QUEUE_ITEM_SIZE 		sizeof( displayData_t )
#define DISP_QUEUE_SIZE 			(DISP_QUEUE_LENGTH * DISP_QUEUE_ITEM_SIZE)

#define PMS_PREHEAT_TIME_IN_SEC 20 /*!<PMS5003 preheat time in seconds, defaults to 45 seconds */

// Static task variables
StackType_t displayTaskStack[DISPLAY_TASK_STACK_SIZE];
StaticTask_t displayTaskBuffer;
TaskHandle_t displayTaskHandle = NULL;

// -- queue handle --
static QueueHandle_t displayTaskQueue;	/*!< DISPLAY Task Events queue */

// -- finite state machine --
state_machine_t dispFSM;

// -- display data instance --
static displayData_t data{};

/*********************************************************
 * @brief function to initialize the display task queue.
 * 
 *********************************************************/
void vTaskDisplay_initDataQueue(void)
{
  if (displayTaskQueue == NULL)
  {
    displayTaskQueue = xQueueCreate(DISP_QUEUE_LENGTH,DISP_QUEUE_ITEM_SIZE);
  }
}

/******************************************************************
 * @brief function to send display events and data to the queue.
 * 
 * @param data 
 * @return BaseType_t 
 ******************************************************************/
BaseType_t tTaskDisplay_sendEvent(displayData_t* data)
{
	return (BaseType_t)xQueueSend(displayTaskQueue, data, 0);
}

/**********************************************************************
 * @brief function to receive display events and data from the queue.
 * 
 * @param data 
 * @param xTicksToWait 
 * @return BaseType_t 
 **********************************************************************/
BaseType_t tTaskDisplay_receiveEvent(displayData_t* data, TickType_t xTicksToWait)
{
	return xQueueReceive(displayTaskQueue, data, xTicksToWait);
}


/*********************************************************************
 * @brief display task function that handles 
 * display events and updates the display accordingly.
 * 
 * @param pvParameters 
 *********************************************************************/
void displayTask(void *pvParameters)
{
  // init finite state machine 
    dispFSM.current_state = DISP_EVENT_WAIT_FOR_EVENT;
    dispFSM.next_state = DISP_EVENT_WAIT_FOR_EVENT;
    dispFSM.returnState = DISP_EVENT_WAIT_FOR_EVENT;
    dispFSM.isFirstTransition = true;

    TickType_t eventWaitTimeout   = portMAX_DELAY;
    displayEvents_t displayEvents = DISP_EVENT_WAIT_FOR_EVENT;
    
  while(1)
  {
    switch(dispFSM.current_state)
    {
      case DISP_EVENT_WAIT_FOR_EVENT:
        if (pdTRUE != tTaskDisplay_receiveEvent(&data, eventWaitTimeout)) 
        {
            dispFSM.next_state = dispFSM.returnState;
        } 
        else 
        {
            displayEvents = data.currentEvent;
            dispFSM.next_state = displayEvents;
        }
        break;
      // set up cases 
      case DISP_EVENT_DEVICE_BOOT:
      {
        vHalDisplay_DrawBoot(&data.sysData.ver);
        dispFSM.next_state = dispFSM.returnState;
        break;
      }
      case DISP_EVENT_WIFI_MAC_ADDR:
      {
        vHalDisplay_drawTwoLines("WIFI MAC ADDRESS:",  data.devInfo.baseMacChr, 6,&data.sysStat,&data.devInfo);
        dispFSM.next_state = dispFSM.returnState;
        break;
      }
      case DISP_EVENT_SD_CARD_INIT:
      {
        vHalDisplay_drawTwoLines("Initializing", "SD Card...", 1,&data.sysStat,&data.devInfo);
        break;
      }
      case DISP_EVENT_CONFIG_READ:
      {
        vHalDisplay_drawTwoLines("SD Card ok!", "Reading config...", 1,&data.sysStat,&data.devInfo);
        break;
      }
      case DISP_EVENT_URL_UPLOAD_STAT:
      {
        vHalDisplay_drawTwoLines("No URL defined!", "No upload!", 6,&data.sysStat,&data.devInfo);
        break;
      }  
      case DISP_EVENT_SD_CARD_NOT_PRESENT:
      {
        vHalDisplay_drawTwoLines("No SD Card!", "No web!", 3,&data.sysStat,&data.devInfo);
        break;
      }
      case DISP_EVENT_SD_CARD_FORMAT:
      {
        vHalDisplay_drawTwoLines("SD Card format!", "No web!", 3,&data.sysStat,&data.devInfo);
        break;
      }
      case DISP_EVENT_SD_CARD_LOG_ERROR:
      {
        vHalDisplay_drawTwoLines("SD Card log", "error!", 3,&data.sysStat,&data.devInfo);
        break;  
      }
      case DISP_EVENT_SD_CARD_CONFIG_CREATE:
      {
        vHalDisplay_drawTwoLines("No cfg found!", "Creating...", 2,&data.sysStat,&data.devInfo);
        break;  
      }
      case DISP_EVENT_SD_CARD_CONFIG_ERROR:
      {
        vHalDisplay_drawTwoLines("Cfg error!", "No web!", 3,&data.sysStat,&data.devInfo);
        break;
      }
      case DISP_EVENT_SD_CARD_CONFIG_INS_DATA:
      {
        vHalDisplay_drawTwoLines("Done! Please", "insert data!", 2,&data.sysStat,&data.devInfo);
        break;
      }
      case DISP_EVENT_SD_CARD_WRITE_DATA:
      {
        vHalDisplay_drawTwoLines("Error while", "writing SD Card!", 2,&data.sysStat,&data.devInfo);
        break;
      }
      case DISP_EVENT_BME680_SENSOR_INIT:
      {
        vHalDisplay_drawTwoLines("Detecting BME680...", "", 0,&data.sysStat,&data.devInfo);
        break;
      }
      case DISP_EVENT_BME680_SENSOR_OKAY:
      {
        vHalDisplay_drawTwoLines("Detecting BME680...", "BME680 -> Ok!",1,&data.sysStat,&data.devInfo);
        break;
      }
      case DISP_EVENT_BME680_SENSOR_ERR:
      {
        vHalDisplay_drawTwoLines("Detecting BME680...", "BME680 -> Err!", 1,&data.sysStat,&data.devInfo);
        break;
      }
      case DISP_EVENT_PMS5003_SENSOR_INIT:
      {
        vHalDisplay_drawTwoLines("Detecting PMS5003...", "", 0,&data.sysStat,&data.devInfo);
        break;
      }
      case DISP_EVENT_PMS5003_SENSOR_OKAY:
      {
        vHalDisplay_drawTwoLines("Detecting PMS5003...", "PMS5003 -> Ok!", 1,&data.sysStat,&data.devInfo);
        break;
      }
      case DISP_EVENT_PMS5003_SENSOR_ERR:
      {
        vHalDisplay_drawTwoLines("Detecting PMS5003...", "PMS5003 -> Err!", 1,&data.sysStat,&data.devInfo);
        break;
      }
      case DISP_EVENT_MICS6814_SENSOR_INIT:
      {
        vHalDisplay_drawTwoLines("Detecting MICS6814...", "", 0,&data.sysStat,&data.devInfo);
        break;
      }
      case DISP_EVENT_MICS6814_SENSOR_OKAY:
      {
        vHalDisplay_drawTwoLines("Detecting MICS6814...", "MICS6814 -> Ok!", 0,&data.sysStat,&data.devInfo);
        break;
      }
      case DISP_EVENT_MICS6814_VALUES_OKAY:
      {
        vHalDisplay_drawLine("MICS6814 values OK!", 1,&data.sysStat,&data.devInfo);
        break;
      }
      case DISP_EVENT_MICS6814_DEF_SETTING:
      {
        vHalDisplay_drawLine("Setting MICS6814...", 1,&data.sysStat,&data.devInfo);
        break;
      }
      case DISP_EVENT_MICS6814_DONE:
      {
        vHalDisplay_drawLine("Done!", 1,&data.sysStat,&data.devInfo);
        break;
      }
      case DISP_EVENT_MICS6814_SENSOR_ERR:
      {
        vHalDisplay_drawTwoLines("Detecting MICS6814...", "MICS6814 -> Err!", 1,&data.sysStat,&data.devInfo);
        break;
      }
      case DISP_EVENT_O3_SENSOR_INIT:
      {
        vHalDisplay_drawTwoLines("Detecting O3...", "", 1,&data.sysStat,&data.devInfo);
        break;
      }
      case DISP_EVENT_O3_SENSOR_OKAY:
      {
        vHalDisplay_drawTwoLines("Detecting O3...", "O3 -> Err!", 0,&data.sysStat,&data.devInfo);
        break;
      }
      case DISP_EVENT_O3_SENSOR_ERR:
      {
        vHalDisplay_drawTwoLines("Detecting O3...", "O3 -> Ok!", 0,&data.sysStat,&data.devInfo);
        break;
      }      
      // loop cases 
      case DISP_EVENT_WAIT_FOR_NETWORK_CONN:
      {
        vHalDisplay_drawTwoLines("Network", "Wait for connection", 0,&data.sysStat,&data.devInfo);
        dispFSM.next_state = dispFSM.returnState;
        break;
      }
      case DISP_EVENT_NETWORK_CONN_FAIL:
      {
        vHalDisplay_drawTwoLines("Network Error", "Failed to connect", 0,&data.sysStat,&data.devInfo);
        dispFSM.next_state = dispFSM.returnState;
        break;
      }
      case DISP_EVENT_READING_SENSORS:
      {
        vHalDisplay_drawTwoLines("Timeout Expired", "Reading Sensors", 0,&data.sysStat,&data.devInfo);
        dispFSM.next_state = dispFSM.returnState;
        break;
      }
      case DISP_EVENT_WAIT_FOR_TIMEOUT:
      {
        char firstRow[17] = {0};
        char secondRow[22] = {0};
        sprintf(firstRow, "meas:%d of %d", data.measStat.measurement_count, data.measStat.avg_measurements);
        sprintf(secondRow, "WAIT %02d:%02d min", (data.measStat.delay_between_measurements - data.measStat.timeout_seconds) / 60, (data.measStat.delay_between_measurements - data.measStat.timeout_seconds) % 60);
        vHalDisplay_drawTwoLines(firstRow, secondRow, 0,&data.sysStat,&data.devInfo);
        dispFSM.next_state = dispFSM.returnState;
        break;
      }
      case DISP_EVENT_PREHEAT_STAT:
      {
        vHalDisplay_drawCountdown(PMS_PREHEAT_TIME_IN_SEC, "Preheating PMS5003...",&data.sysStat,&data.devInfo);
        dispFSM.next_state = dispFSM.returnState;
        break;
      }
      case DISP_EVENT_MEAS_IN_PROGRESS:
      {
        vHalDisplay_drawTwoLines("Measurements", "in progress...", 0,&data.sysStat,&data.devInfo);
        dispFSM.next_state = dispFSM.returnState;
        break;
      }
      case DISP_EVENT_SENDING_MEAS:
      {
        vHalDisplay_drawTwoLines("All measurements", "obtained, sending...", 0,&data.sysStat,&data.devInfo);
        dispFSM.next_state = dispFSM.returnState;
        break;
      }
      case DISP_EVENT_SYSTEM_ERROR:
      {
        vHalDisplay_drawTwoLines("System in error!", "Waiting for reset...", 10,&data.sysStat,&data.devInfo);
        dispFSM.next_state = dispFSM.returnState;
        break;
      }
      // network cases
      case DISP_EVENT_CONN_TO_WIFI:
      {
        vHalDisplay_drawTwoLines("Connecting to", "WiFi...", 1,&data.sysStat,&data.devInfo);
        break;
      }
      case DISP_EVENT_CONN_TO_GPRS:
      {
        vHalDisplay_drawTwoLines("Connecting to", "GPRS...", 1,&data.sysStat,&data.devInfo);
        break;
      }
      case DISP_EVENT_RETREIVE_DATETIME:
      {
        vHalDisplay_drawTwoLines("Getting date&time...", "Please wait...", 1,&data.sysStat,&data.devInfo);
        break;
      }
      case DISP_EVENT_DATETIME_OK:
      {
        vHalDisplay_drawTwoLines("Getting date&time...", "OK!", 1,&data.sysStat,&data.devInfo);
        break;
      }
      case DISP_EVENT_DATETIME:
      {
        vHalDisplay_drawTwoLines("Date & Time:", data.sysData.currentDataTime.c_str(), 0,&data.sysStat,&data.devInfo);
        break;
      }
      case DISP_EVENT_DATETIME_ERR:
      {
        vHalDisplay_drawTwoLines("Date & time err!", "Is internet ok?", 0,&data.sysStat,&data.devInfo);
        break;
      }
      case DISP_EVENT_WIFI_CONNECTED:
      {
        break;
      }
      case DISP_EVENT_WIFI_DISCONNECTED:
      {
        vHalDisplay_drawLine("WiFi connect err!", 2,&data.sysStat,&data.devInfo);
        break;
      }
      case DISP_EVENT_SSID_NOT_FOUND:
      {
        vHalDisplay_drawLine(data.devInfo.noNet.c_str(), 2,&data.sysStat,&data.devInfo);
        break;
      }
      case DISP_EVENT_NO_NETWORKS_FOUND:
      {
        vHalDisplay_drawLine("No networks found!", 2,&data.sysStat,&data.devInfo);
        break;
      }
      case DISP_EVENT_CONN_RETRY:
      {
        vHalDisplay_drawTwoLines("Retrying...", data.devInfo.remain.c_str(), 2,&data.sysStat,&data.devInfo);
        break;
      }
      case DISP_EVENT_NO_INTERNET:
      {
        vHalDisplay_drawLine("No internet!", 2,&data.sysStat,&data.devInfo);
        break;
      }

      //modem 
      case DISP_EVENT_SIM_ERROR:
      {
        vHalDisplay_drawTwoLines("ERROR:", "NO SIM!", 3,&data.sysStat,&data.devInfo);
        break;
      }
      case DISP_EVENT_NETWORK_ERROR:
      {
        vHalDisplay_drawTwoLines("ERROR:", "NO NETWORK!", 3,&data.sysStat,&data.devInfo);
        break;
      }
      case DISP_EVENT_GPRS_ERROR:
      {
        vHalDisplay_drawTwoLines("ERROR:", "NO GPRS!", 3,&data.sysStat,&data.devInfo);
        break;
      }
      // default case
      default:
        dispFSM.current_state = dispFSM.next_state = dispFSM.returnState;
    }
    dispFSM.current_state = dispFSM.next_state;
  }
}

/******************************************************
 * @brief   Function to create the display task.
 * 
 ******************************************************/
void vTaskDisplay_createTask(void)
{
  displayTaskHandle = xTaskCreateStaticPinnedToCore(
                                                  displayTask,              // Task function
                                                  "displayTask",            // Name
                                                  DISPLAY_TASK_STACK_SIZE,  // Stack size
                                                  NULL,                     // Parameters
                                                  DISPLAY_TASK_PRIORITY,    // Priority
                                                  displayTaskStack,         // Stack buffer
                                                  &displayTaskBuffer,       // Task buffer
                                                  1                         // Core 1
                                              );
}


