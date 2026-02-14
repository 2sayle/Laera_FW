/**
  ******************************************************************************
  * @file    sensor_task.h
  * @brief   Public interface for sensor acquisition task.
  ******************************************************************************
  * @attention
  *
  * This header declares task context/data structures and task entry points used
  * for sensor acquisition and data publishing.
  *
  ******************************************************************************
  */

#ifndef LAERA_FW_SENSOR_TASK_H
#define LAERA_FW_SENSOR_TASK_H

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"


/* Exported types ------------------------------------------------------------*/
/**
  * @brief Runtime context for sensor task control and communication.
  */
typedef struct {
    TaskHandle_t task_handle;
    QueueHandle_t data_queue;
    SemaphoreHandle_t config_mutex;
    bool stop_requested;
} sensor_task_ctx_t;

/**
  * @brief Sensor sample payload.
  */
typedef struct {
    float temperature;
    float humidity;
    float pressure;
    float gas_resistance;
} sensor_data_t;

/* Exported functions prototypes ---------------------------------------------*/
/**
  * @brief  Sensor acquisition task.
  * @param  arg Task argument pointer.
  * @retval None
  */
void sensor_task(void *arg);

/**
  * @brief  Sensor data publisher task.
  * @param  arg Task argument pointer.
  * @retval None
  */
void publisher_task(void *arg);

#endif //LAERA_FW_SENSOR_TASK_H
