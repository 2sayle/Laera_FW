#include <stdio.h>
#include <driver/i2c_master.h>
#include <driver/i2c_types.h>

#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "driver/i2c.h"

#include "bme68x.h"
#include "bme68x_defs.h"
#include "drv_bme680.h"


/* ------- Global variables ------- */
struct bme68x_dev bme = {0};
static i2c_master_bus_handle_t i2cMasterBus = NULL;
const uint8_t bme_addr = BME680_I2C_ADDR_1;

static const char *TAG = "APP MAIN";

/**
 * @brief DeInitialize I2C master bus
 *
 * @return esp_err_t
 */
esp_err_t i2c_master_deinit(void) {
    if (!i2cMasterBus) return ESP_OK;
    esp_err_t err = i2c_del_master_bus(i2cMasterBus);
    if (err == ESP_OK) {
        i2cMasterBus = NULL;
        ESP_LOGI(TAG, "I2C master bus deinitialized");
    } else {
        ESP_LOGE(TAG, "i2c_del_master_bus failed: %s", esp_err_to_name(err));
    }
    return err;
}

/**
 * @brief Write 1 byte to register
 * 
 * @param dev 
 * @param reg 
 * @param val 
 * @return esp_err_t 
 */
static esp_err_t bme680_write_byte(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(dev, buf, sizeof(buf), -1);
}

/** @brief Read N bytes starting at register 
* 
* @param dev 
* @param start_reg 
* @param out 
* @param len 
* @return esp_err_t 
*/
static esp_err_t bme680_read(i2c_master_dev_handle_t dev, uint8_t start_reg, uint8_t *out, size_t len)
{
    if (!out || len == 0) return ESP_ERR_INVALID_ARG;
    return i2c_master_transmit_receive(dev, &start_reg, 1, out, len, -1);
}


/**
 * @brief Log hexadecimal data
 * 
 * @param label 
 * @param data 
 * @param len 
 */
static void log_hex(const char *label, const uint8_t *data, size_t len)
{
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


/**
 *

void app_main(void) {
    
    ESP_ERROR_CHECK(i2c_master_init());

    i2c_master_dev_handle_t bme = NULL;
    ESP_ERROR_CHECK(i2c_add_device(bme_addr, &bme));

    esp_err_t err = i2c_master_probe(i2cMasterBus, bme_addr, 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_master_probe failed: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
    }

    // --- Read CHIP_ID ---
    uint8_t chip_id = 0;
    err = bme680_read(bme, BME680_REG_CHIP_ID, &chip_id, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "CHIP_ID read failed: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
    }
}*/

void app_main(void) {
    struct bme68x_i2c_ctx i2c_ctx = {NULL, NULL};

    esp_err_t err = bm68x_i2c_init_itf(&bme, &i2c_ctx);

    int8_t rslt = bme68x_init(&bme);

    rslt = bme68x_selftest_check(&bme);

    if (rslt == BME68X_OK) {
        ESP_LOGI(TAG,"Self-test passed\n");
    }

    if (rslt == BME68X_E_SELF_TEST) {
        ESP_LOGI(TAG, "Self-test failed\n");
    }

    ESP_LOGI(TAG, "BME68X_OK\n");

}
