/**
 * @file tasks.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-07-20
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/idf_additions.h"
#include "freertos/portmacro.h"


#include "display_task.h"
#include "msp_hal/display/display.h"
//--------------------------------------------------------------------------------------------------
//------------------ DISPLAY TASK SECTION ----------------------------------------------------------
//--------------------------------------------------------------------------------------------------



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

static QueueHandle_t displayTaskQueue;	/*!< DISPLAY Task Events queue */

state_machine_t dispFSM;

displayData_t data;


// -- queue initialization --
void initDisplayDataQueue()
{
  if (displayTaskQueue == NULL)
  {
    displayTaskQueue = xQueueCreate(DISP_QUEUE_LENGTH,DISP_QUEUE_ITEM_SIZE);
  }
}

// -- event send-receive -- 
inline BaseType_t tMsp_sendDisplayEvent(displayData_t* data)
{
	return (BaseType_t)xQueueSend(displayTaskQueue, data, 0);
}

inline BaseType_t tMsp_receiveDisplayEvent(displayData_t* data, TickType_t xTicksToWait)
{
	return xQueueReceive(displayTaskQueue, data, xTicksToWait);
}



void displayTask(void *pvParameters)
{
  // init finite state machine 
    dispFSM.current_state = DISP_EVENT_WAIT_FOR_EVENT;
    dispFSM.next_state = DISP_EVENT_WAIT_FOR_EVENT;
    dispFSM.returnState = DISP_EVENT_WAIT_FOR_EVENT;
    dispFSM.isFirstTransition = true;

    Ticktype_t eventWaitTimeout   = portMAX_DELAY;
    displayEvents_t displayEvents = DISP_EVENT_WAIT_FOR_EVENT;

    memset(&data,0,sizeof(displayData_t));
    
  while(1)
  {
    switch(dispFSM.current_state)
    {
      case DISP_EVENT_WAIT_FOR_EVENT:
        if (pdTRUE != tMsp_receiveDisplayEvent(&data, eventWaitTimeout)) 
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
        drawBoot(&data.sysData.ver);
        dispFSM.next_state = dispFSM.returnState;
        break;
      case DISP_EVENT_WIFI_MAC_ADDR:
        drawTwoLines("WIFI MAC ADDRESS:",  data.devInfo.baseMacChr, 6,&data.sysStat,&data.devInfo);
        dispFSM.next_state = dispFSM.returnState;
        break;
      case DISP_EVENT_SD_CARD_INIT:
        drawTwoLines("Initializing", "SD Card...", 1,&data.sysStat,&data.devInfo);
        break;
      case DISP_EVENT_CONFIG_READ:
        drawTwoLines("SD Card ok!", "Reading config...", 1,&data.sysStat,&data.devInfo);
        break;
      case DISP_EVENT_URL_UPLOAD_STAT:
        drawTwoLines("No URL defined!", "No upload!", 6,&data.sysStat,&data.devInfo);
        break;  
      case DISP_EVENT_SD_CARD_NOT_PRESENT:
        drawTwoLines("No SD Card!", "No web!", 3,&data.sysStat,&data.devInfo);
        break;
      case DISP_EVENT_SD_CARD_FORMAT:
        drawTwoLines("SD Card format!", "No web!", 3,&data.sysStat,&data.devInfo);
        break;
      case DISP_EVENT_BME680_SENSOR_INIT:
        drawTwoLines("Detecting BME680...", "", 0,&data.sysStat,&data.devInfo);
        break;
      case DISP_EVENT_BME680_SENSOR_OKAY:
        drawTwoLines("Detecting BME680...", "BME680 -> Ok!",1,&data.sysStat,&data.devInfo);
        break;
      case DISP_EVENT_BME680_SENSOR_ERR:
        drawTwoLines("Detecting BME680...", "BME680 -> Err!", 1,&data.sysStat,&data.devInfo);
        break;
      case DISP_EVENT_PMS5003_SENSOR_INIT:
        drawTwoLines("Detecting PMS5003...", "", 0,&data.sysStat,&data.devInfo);
        break;
      case DISP_EVENT_PMS5003_SENSOR_OKAY:
        drawTwoLines("Detecting PMS5003...", "PMS5003 -> Ok!", 1,&data.sysStat,&data.devInfo);
        break;
      case DISP_EVENT_PMS5003_SENSOR_ERR:
        drawTwoLines("Detecting PMS5003...", "PMS5003 -> Err!", 1,&data.sysStat,&data.devInfo);
        break;
      case DISP_EVENT_MICS6814_SENSOR_INIT:
        drawTwoLines("Detecting MICS6814...", "", 0,&data.sysStat,&data.devInfo);
        break;
      case DISP_EVENT_MICS6814_SENSOR_OKAY:
        drawTwoLines("Detecting MICS6814...", "MICS6814 -> Ok!", 0,&data.sysStat,&data.devInfo);
        break;
      case DISP_EVENT_MICS6814_VALUES_OKAY:
        drawLine("MICS6814 values OK!", 1,&data.sysStat,&data.devInfo);
        break;
      case DISP_EVENT_MICS6814_DEF_SETTING:
        drawLine("Setting MICS6814...", 1,&data.sysStat,&data.devInfo);
        break;
      case DISP_EVENT_MICS6814_DONE:
        drawLine("Done!", 1,&data.sysStat,&data.devInfo);
        break;
      case DISP_EVENT_MICS6814_SENSOR_ERR:
        drawTwoLines("Detecting MICS6814...", "MICS6814 -> Err!", 1,&data.sysStat,&data.devInfo);
        break;
      case DISP_EVENT_O3_SENSOR_INIT:
        drawTwoLines("Detecting O3...", "", 1,&data.sysStat,&data.devInfo);
        break;
      case DISP_EVENT_O3_SENSOR_OKAY:
        drawTwoLines("Detecting O3...", "O3 -> Err!", 0,&data.sysStat,&data.devInfo);
        break;
      case DISP_EVENT_O3_SENSOR_ERR:
        drawTwoLines("Detecting O3...", "O3 -> Ok!", 0,&data.sysStat,&data.devInfo);
        break;      
      // loop cases 
      case DISP_EVENT_WAIT_FOR_NETWORK_CONN:
        drawTwoLines("Network", "Wait for connection", 0,&data.sysStat,&data.devInfo);
        dispFSM.next_state = dispFSM.returnState;
        break;
      case DISP_EVENT_NETWORK_CONN_FAIL:
        drawTwoLines("Network Error", "Failed to connect", 0,&data.sysStat,&data.devInfo);
        dispFSM.next_state = dispFSM.returnState;
        break;
      case DISP_EVENT_READING_SENSORS:
        drawTwoLines("Timeout Expired", "Reading Sensors", 0,&data.sysStat,&data.devInfo);
        dispFSM.next_state = dispFSM.returnState;
        break;
      case DISP_EVENT_WAIT_FOR_TIMEOUT:
        char firstRow[17] = {0};
        char secondRow[22] = {0};
        sprintf(firstRow, "meas:%d of %d", data.measStat.measurement_count, data.measStat.avg_measurements);
        sprintf(secondRow, "WAIT %02d:%02d min", (data.measStat.delay_between_measurements - data.measStat.timeout_seconds) / 60, (data.measStat.delay_between_measurements - data.measStat.timeout_seconds) % 60);
        drawTwoLines(firstRow, secondRow, 0,&data.sysStat,&data.devInfo);
        dispFSM.next_state = dispFSM.returnState;
        break;
      case DISP_EVENT_PREHEAT_STAT:
        drawCountdown(PMS_PREHEAT_TIME_IN_SEC, "Preheating PMS5003...",&data.sysStat,&data.devInfo);
        dispFSM.next_state = dispFSM.returnState;
        break;
      case DISP_EVENT_MEAS_IN_PROGRESS:
        drawTwoLines("Measurements", "in progress...", 0,&data.sysStat,&data.devInfo);
        dispFSM.next_state = dispFSM.returnState;
        break;
      case DISP_EVENT_SENDING_MEAS:
        drawTwoLines("All measurements", "obtained, sending...", 0,&data.sysStat,&data.devInfo);
        dispFSM.next_state = dispFSM.returnState;
        break;
      case DISP_EVENT_SYSTEM_ERROR:
        drawTwoLines("System in error!", "Waiting for reset...", 10,&data.sysStat,&data.devInfo);
        dispFSM.next_state = dispFSM.returnState;
        break;
      // network cases
      case DISP_EVENT_CONN_TO_WIFI:
        drawTwoLines("Connecting to", "WiFi...", 1,&data.sysStat,&data.devInfo);
        break;
      case DISP_EVENT_CONN_TO_GPRS:
        drawTwoLines("Connecting to", "GPRS...", 1,&data.sysStat,&data.devInfo);
        break;
      case DISP_EVENT_RETREIVE_DATETIME:
        drawTwoLines("Getting date&time...", "Please wait...", 1,&data.sysStat,&data.devInfo);
        break;
      case DISP_EVENT_DATETIME_OK:
        drawTwoLines("Getting date&time...", "OK!", 1,&data.sysStat,&data.devInfo);
        break;
      case DISP_EVENT_DATETIME:
        drawTwoLines("Date & Time:", data.sysData.currentDataTime.c_str(), 0,&data.sysStat,&data.devInfo);
        break;
      case DISP_EVENT_DATETIME_ERR:
        drawTwoLines("Date & time err!", "Is internet ok?", 0,&data.sysStat,&data.devInfo);
        break;
      case DISP_EVENT_WIFI_CONNECTED:
        break;
      case DISP_EVENT_WIFI_DISCONNECTED:
        drawLine("WiFi connect err!", 2,&data.sysStat,&data.devInfo);
        break;
      case DISP_EVENT_SSID_NOT_FOUND:
        drawLine(&data.devInfo.noNet.c_str(), 2,&data.sysStat,&data.devInfo);
        break;
      case DISP_EVENT_NO_NETWORKS_FOUND:
        drawLine("No networks found!", 2,&data.sysStat,&data.devInfo);
        break;
      case DISP_EVENT_CONN_RETRY:
        drawTwoLines("Retrying...", &data.devInfo.remain.c_str(), 2,&data.sysStat,&data.devInfo);
        break;
      case DISP_EVENT_NO_INTERNET:
        drawLine("No internet!", 2,&data.sysStat,&data.devInfo);
        break;

      //modem 
      case DISP_EVENT_SIM_ERROR:
        drawTwoLines("ERROR:", "NO SIM!", 3,&data.sysStat,&data.devInfo);
        break;
      case DISP_EVENT_NETWORK_ERROR:
        drawTwoLines("ERROR:", "NO NETWORK!", 3,&data.sysStat,&data.devInfo);
        break;
      case DISP_EVENT_GPRS_ERROR:
        drawTwoLines("ERROR:", "NO GPRS!", 3,&data.sysStat,&data.devInfo);
        break;
      default:
        dispFSM.current_state = dispFSM.next_state = dispFSM.returnState;
    }
    dispFSM.current_state = dispFSM.next_state
  }
}

void vTaskDisplay_createTask(void)
{
    // Initialize the send data task
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


