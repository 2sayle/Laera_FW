//
// Created by Elyass Jaoudat on 08/01/2026.
//

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "bme68x.h"
#include "bme68x_defs.h"
#include "sensor_task.h"

extern struct bme68x_dev bme;

void sensor_task(void *arg) {
    while (1) {

        /* Local variables */
        sensor_data_t sample = {0};
        bme68x_data data = {0};
        uint8_t nData = 0;

        /* Read raw sensor data */
        bme68x_get_data(BME68X_FORCED_MODE, &data, &nData, &bme);
        
    }
}