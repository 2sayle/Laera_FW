#include <stdio.h>
#include <driver/i2c_master.h>
#include <driver/i2c_types.h>

#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "driver/i2c.h"

#include "bme68x.h"
#include "bme68x_defs.h"
#include "drv_bme680.h"
#include "sensor_task.h"

#define DEBUG 1

/* Global variables ------------------------------------------------------------------------------------------- */
struct bme68x_dev bme = {0};
static struct bme68x_i2c_ctx i2c_ctx = {0};
static i2c_master_bus_handle_t i2cMasterBus = NULL;
const uint8_t bme_addr = BME680_I2C_ADDR_1;

static const char TAG[] = "APP MAIN";

/* Queues */
static const uint8_t QueueLength = 8;
QueueHandle_t MainQueue = NULL;

/* Static functions ------------------------------------------------------------------------------------------- */
static void log_hex(const char *label, const uint8_t *data, size_t len) {
    char line[3 * 32 + 1]; // up to 32 bytes per line
    size_t i = 0;
    while (i < len) {
        size_t chunk = (len - i > 32) ? 32 : (len - i);
        size_t pos = 0;
        for (size_t j = 0; j < chunk; j++) {
            pos += snprintf(&line[pos], sizeof(line) - pos, "%02X ", data[i + j]);
        }
        line[pos ? pos - 1 : 0] = '\0';
        ESP_LOGI(TAG, "%s[%u..%u]: %s", label, (unsigned)i, (unsigned)(i + chunk - 1), line);
        i += chunk;
    }
}


/*
 * Application main entry point
 */
 
void app_main(void) {
#ifdef DEBUG
    ESP_LOGW("BOOT", "Reset reason: %d", esp_reset_reason());
#endif

    esp_err_t err = bm68x_i2c_init_itf(&bme, &i2c_ctx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C Bus initialization failed\n");
        return;
    }

    int8_t rslt = bme68x_init(&bme);
    if (rslt != BME68X_OK) {
        ESP_LOGE(TAG, "BME68X initialization failed\n");
        return;
    }
    ESP_LOGI(TAG, "BME68X initialization successful\n");

    rslt = bme68x_selftest_check(&bme);
    if (rslt != BME68X_OK) {
        ESP_LOGE(TAG, "Self-test failed\n");
    }
    ESP_LOGI(TAG, "Self-test OK");

    /* Create the main queue */
    MainQueue = xQueueCreate(QueueLength, sizeof(sensor_data_t));
    if (MainQueue == NULL) {
        ESP_LOGE(TAG, "Failed to create MainQueue");
        return;
    }

    /* Create sensor task */
    xTaskCreate
        (sensor_task,   // Task function
        "sensor_task",  // Task name
        4096,           // Stack size
        NULL,           // Task parameters
        5,              // Task priority
        NULL);          // Task handle

    ESP_LOGI(TAG, "Application started");

}
