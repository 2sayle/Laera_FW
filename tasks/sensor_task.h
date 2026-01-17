//
// Created by Elyass Jaoudat on 08/01/2026.
//

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


#endif //LAERA_FW_SENSOR_TASK_H