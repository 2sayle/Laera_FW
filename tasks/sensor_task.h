/**
 * @file sensor_task.h
 * @author Elyass Jaoudat (ejaoudat@outlook.fr)
 * @brief Header file for the sensor task, which reads data from the BME68x sensor and publishes it to a queue.
 * @version 0.1
 * @date 2026-02-11
 *
 */

#ifndef LAERA_FW_SENSOR_TASK_H
#define LAERA_FW_SENSOR_TASK_H

#include "sensor_task.h"


/* ----------------------------- Data structures ---------------------------------- */
typedef struct {
    TaskHandle_t task_handle;
    QueueHandle_t data_queue;
    SemaphoreHandle_t config_mutex;
    bool stop_requested;
} sensor_task_ctx_t;

typedef struct {
    float temperature;
    float humidity;
    float pressure;
    float gas_resistance;
} sensor_data_t;

/* ----------------------------- Function prototypes ---------------------------------- */

void sensor_task(void *arg);
void publisher_task(void *arg);

#endif //LAERA_FW_SENSOR_TASK_H