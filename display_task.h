/**
 * @file library.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-07-09
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#ifndef TASKS_H
#define TASKS_H

#include "shared_values.h"
#include "freertos/portmacro.h"

// -- display events --
typedef enum _DISPLAY_EVENTS_ {
  DISP_EVENT_WAIT_FOR_EVENT,
  // set up cases 
  DISP_EVENT_DEVICE_BOOT,
  DISP_EVENT_WIFI_MAC_ADDR,

  DISP_EVENT_SD_CARD_INIT,
  DISP_EVENT_CONFIG_READ,
  DISP_EVENT_URL_UPLOAD_STAT,
  DISP_EVENT_SD_CARD_NOT_PRESENT,
  DISP_EVENT_SD_CARD_FORMAT,

  DISP_EVENT_BME680_SENSOR_INIT,
  DISP_EVENT_BME680_SENSOR_OKAY,
  DISP_EVENT_BME680_SENSOR_ERR,

  DISP_EVENT_PMS5003_SENSOR_INIT,
  DISP_EVENT_PMS5003_SENSOR_OKAY,
  DISP_EVENT_PMS5003_SENSOR_ERR,

  DISP_EVENT_MICS6814_SENSOR_INIT,
  DISP_EVENT_MICS6814_SENSOR_OKAY,
  DISP_EVENT_MICS6814_VALUES_OKAY,
  DISP_EVENT_MICS6814_DEF_SETTING,
  DISP_EVENT_MICS6814_DONE,
  DISP_EVENT_MICS6814_SENSOR_ERR,

  DISP_EVENT_O3_SENSOR_INIT,
  DISP_EVENT_O3_SENSOR_OKAY,
  DISP_EVENT_O3_SENSOR_ERR,
  
  // loop cases 
  DISP_EVENT_WAIT_FOR_NETWORK_CONN,
  DISP_EVENT_NETWORK_CONN_FAIL,
  DISP_EVENT_READING_SENSORS,
  DISP_EVENT_WAIT_FOR_TIMEOUT,
  DISP_EVENT_PREHEAT_STAT,
  DISP_EVENT_MEAS_IN_PROGRESS,
  DISP_EVENT_SENDING_MEAS,
  DISP_EVENT_SYSTEM_ERROR,

  // network cases
  DISP_EVENT_CONN_TO_WIFI,
  DISP_EVENT_CONN_TO_GPRS,
  DISP_EVENT_RETREIVE_DATETIME,
  DISP_EVENT_DATETIME_OK,
  DISP_EVENT_DATETIME,
  DISP_EVENT_DATETIME_ERR,

  // wi-fi
  DISP_EVENT_WIFI_CONNECTED,
  DISP_EVENT_WIFI_DISCONNECTED,
  DISP_EVENT_SSID_NOT_FOUND,
  DISP_EVENT_NO_NETWORKS_FOUND,
  DISP_EVENT_CONN_RETRY,
  DISP_EVENT_NO_INTERNET,

  //modem 
  DISP_EVENT_SIM_ERROR,
  DISP_EVENT_NETWORK_ERROR,
  DISP_EVENT_GPRS_ERROR,

}displayEvents_t;


// -- display task queue data
typedef struct _DISP_TASK_DATA_{
  displayEvents_t currentEvent; 
  sensorData_t sensorData;       
  systemStatus_t sysStat;           
  deviceNetworkInfo_t devInfo;
  systemData_t sysData;
  deviceMeasurement_t measStat;
}displayData_t;



void initDisplayDataQueue();
void vTaskDisplay_createTask(void);

BaseType_t tMsp_sendDisplayEvent(displayData_t* data);
BaseType_t tMsp_receiveDisplayEvent(displayData_t* data, TickType_t xTicksToWait);


#endif