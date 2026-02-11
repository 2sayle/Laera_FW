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
#include "wifi_handler.h"

#define DEBUG 1
#define QUEUE_LENGTH 10

/* Global variables ------------------------------------------------------------------------------------------- */

/* I2C bus */
struct bme68x_dev bme = {0};
static struct bme68x_i2c_ctx i2c_ctx = {0};
static i2c_master_bus_handle_t i2cMasterBus = NULL;
const uint8_t bme_addr = BME680_I2C_ADDR_1;

/* Sensor */
QueueHandle_t MetricsQueue;
sensor_task_ctx_t SensorTaskCtx = {
    .data_queue = NULL,
    .task_handle = NULL,
    .config_mutex = NULL,
    .stop_requested = false,
};

/* Tag */
static const char TAG[] = "APP MAIN";

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

    /* Initialize I2C bus, sensor and perform a quick self-test */
    if (bme68x_startup(&bme, &i2c_ctx) != ESP_OK) return;

    /* Create the main queue */
    MetricsQueue = xQueueCreate(QUEUE_LENGTH, sizeof(sensor_data_t));
    if (MetricsQueue == NULL) {
        ESP_LOGE(TAG, "Failed to create MainQueue");
        return;
    }

    /* Create sensor task */
    xTaskCreate(sensor_task,   // Task function
        "sensor_task",  // Task name
        2048,           // Stack size
        MetricsQueue,   // Task parameters
        2,              // Task priority
        NULL);          // Task handle

    xTaskCreatePinnedToCore(wifi_task,
        "wifi_task",
        4096,
        MetricsQueue,
        5,
        NULL,
        0); // Core 0



    ESP_LOGI(TAG, "Application started");

}
