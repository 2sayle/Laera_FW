//
// Created by Elyass Jaoudat on 08/01/2026.
//

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "bme68x.h"
#include "bme68x_defs.h"
#include "sensor_task.h"
#include "esp_log.h"

extern struct bme68x_dev bme;

static const char TAG[] = "SENSOR_TASK";

void sensor_task(void *arg) {
    struct bme68x_conf conf = {
        .os_hum = BME68X_OS_2X,
        .os_temp = BME68X_OS_4X,
        .os_pres = BME68X_OS_4X,
        .filter = BME68X_FILTER_SIZE_3,
        .odr = BME68X_ODR_NONE,
    };
    struct bme68x_heatr_conf heatr_conf = {
        .enable = BME68X_ENABLE,
        .heatr_temp = 300,
        .heatr_dur = 100,
    };

    int8_t rslt = bme68x_set_conf(&conf, &bme);
    if (rslt != BME68X_OK) {
        ESP_LOGE(TAG, "Failed to set sensor config: %i", rslt);
    }
    rslt = bme68x_set_heatr_conf(BME68X_FORCED_MODE, &heatr_conf, &bme);
    if (rslt != BME68X_OK) {
        ESP_LOGE(TAG, "Failed to set heater config: %i", rslt);
    }

    while (1) {

        /* Local variables */
        sensor_data_t sample = {0};
        struct bme68x_data data = {0};
        uint8_t nData = 0;

        /* Trigger a forced measurement */
        rslt = bme68x_set_op_mode(BME68X_FORCED_MODE, &bme);
        if (rslt != BME68X_OK) {
            ESP_LOGE(TAG, "Failed to set op mode: %i", rslt);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        uint32_t meas_us = bme68x_get_meas_dur(BME68X_FORCED_MODE, &conf, &bme) +
            ((uint32_t)heatr_conf.heatr_dur * 1000U);
        vTaskDelay(pdMS_TO_TICKS((meas_us + 999) / 1000));

        /* Read raw sensor data */
        rslt = bme68x_get_data(BME68X_FORCED_MODE, &data, &nData, &bme);
        if (rslt == BME68X_OK && nData > 0) {
            /* Process and store the data */
            sample.temperature = data.temperature;
            sample.pressure = data.pressure;
            sample.humidity = data.humidity;
            sample.gas_resistance = data.gas_resistance;

            /* Log the sensor data */
            ESP_LOGI(TAG, "Temperature: %.2f, Pressure: %.2f, Humidity: %.2f, Gas Resistance: %.2f",
                     sample.temperature, sample.pressure, sample.humidity, sample.gas_resistance);

            /* Send the data to the queue
            xQueueSend(arg, &sample, portMAX_DELAY);*/
        } else if (rslt == BME68X_W_NO_NEW_DATA || nData == 0) {
            ESP_LOGD(TAG, "No new data available");
        } else {
            ESP_LOGE(TAG, "Failed to read sensor data: %i", rslt);
        }

        /* Wait before the next reading */
        vTaskDelay(pdMS_TO_TICKS(5000)); // 5 seconds
    }
}
