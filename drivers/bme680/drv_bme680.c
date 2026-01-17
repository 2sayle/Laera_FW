#include <drv_bme680.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "driver/i2c_master.h"
#include "driver/i2c_types.h"

#include "esp_err.h"
#include "esp_rom_sys.h"
#include "esp_log.h"

#include "macros.h"
#include "bme68x.h"
#include "drv_bme680.h"

#include "../../../../../../../ESP/v5.5.1/esp-idf/components/esp_driver_i2c/i2c_private.h"

char *TAG = "I2C DRIVER";


/* Mapping READ function for BME68X driver */
static BME68X_INTF_RET_TYPE bme68x_i2c_read(uint8_t reg_addr,
                                            uint8_t *reg_data,
                                            uint32_t length,
                                            void *intf_ptr)
{
    struct bme68x_i2c_ctx *ctx = intf_ptr;

    if (!ctx || !ctx->dev || !reg_data || length == 0) {
        return (BME68X_INTF_RET_TYPE)-1;
    }

    esp_err_t err = i2c_master_transmit_receive(ctx->dev, &reg_addr, 1, reg_data, length, -1);
    return (err == ESP_OK) ? BME68X_INTF_RET_SUCCESS : (BME68X_INTF_RET_TYPE)-1;
}

/* Mapping WRITE function for BME68X driver */
static BME68X_INTF_RET_TYPE bme68x_i2c_write(uint8_t reg_addr,
                                             const uint8_t *reg_data,
                                             uint32_t length,
                                             void *intf_ptr)
{
    struct bme68x_i2c_ctx *ctx = intf_ptr;

    if (!ctx || !ctx->dev || !reg_data || length == 0) {
        return (BME68X_INTF_RET_TYPE)-1;
    }

    uint8_t *buf = malloc(length + 1);
    if (!buf) {
        return (BME68X_INTF_RET_TYPE)-1;
    }

    buf[0] = reg_addr;
    memcpy(&buf[1], reg_data, length);

    esp_err_t err = i2c_master_transmit(ctx->dev, buf, length + 1, -1);
    free(buf);

    return (err == ESP_OK) ? BME68X_INTF_RET_SUCCESS : (BME68X_INTF_RET_TYPE)-1;
}

static void bme68x_delay_us(uint32_t period, void *intf_ptr)
{
    (void)intf_ptr;
    if (period == 0) {
        return;
    }
    BLOCKING_DELAY_US(period);
}


static esp_err_t bme68x_i2c_init_dev(struct bme68x_dev *dev,
                         struct bme68x_i2c_ctx *ctx)
{
    if (!dev || !ctx) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(dev, 0, sizeof(*dev));
    dev->intf = BME68X_I2C_INTF;
    dev->intf_ptr = ctx;
    dev->read = bme68x_i2c_read;
    dev->write = bme68x_i2c_write;
    dev->delay_us = bme68x_delay_us;
    dev->amb_temp = 25;
    dev->intf_rslt = BME68X_INTF_RET_SUCCESS;
    dev->info_msg = 0;
    dev->variant_id = BME68X_VARIANT_GAS_LOW;

    return ESP_OK;
}



esp_err_t bm68x_i2c_init_itf(struct bme68x_dev *handle, struct bme68x_i2c_ctx *ctx) {

    /* Check parameters */
    if (ctx->bus != NULL) {
        ESP_LOGW(TAG, "I2C Bus already initialized");
        return ESP_OK;
    }

    bme68x_i2c_init_dev(handle, ctx);


    /* Master Bus config */
    i2c_master_bus_config_t busCfg = {
        .i2c_port = I2C_PORT_NUM,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {
            .enable_internal_pullup = 0,
        },
    };

    /* Device config */
    i2c_device_config_t devCfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BME680_I2C_ADDR_1,
        .scl_speed_hz = I2C_CLK_HZ,
    };

    /* Initialize I2C Master Bus */
    esp_err_t err = i2c_new_master_bus(&busCfg, &ctx->bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    /* Register BME680 device on the bus */
    err = i2c_master_bus_add_device(ctx->bus, &devCfg, &ctx->dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_master_bus_add_device failed: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    /* Probe the I2C device to detect its presence on the bus */
    err = i2c_master_probe(ctx->bus, devCfg.device_address, 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_master_probe failed: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    /* Read Chip ID to confirm it's the right sensor */
    uint8_t chip_id = 0;
    BME68X_INTF_RET_TYPE status = bme68x_i2c_read(ctx->dev->device_address, &chip_id, 1, ctx);
    if (status != BME68X_INTF_RET_SUCCESS) {
        ESP_LOGE(TAG, "CHIP_ID read failed: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }


    ESP_LOGI(TAG, "I2C master bus initialized");
    return ESP_OK;

}


void bme68x_check_rslt(const char api_name[], int8_t rslt)
{
    switch (rslt)
    {
    case BME68X_OK:

        /* Do nothing */
        break;
    case BME68X_E_NULL_PTR:
        ESP_LOGI("[BME680]", "API name [%s]  Error [%d] : Null pointer\r\n", api_name, rslt);
        break;
    case BME68X_E_COM_FAIL:
        ESP_LOGI("[BME680]", "API name [%s]  Error [%d] : Communication failure\r\n", api_name, rslt);
        break;
    case BME68X_E_INVALID_LENGTH:
        ESP_LOGI("[BME680]", "API name [%s]  Error [%d] : Incorrect length parameter\r\n", api_name, rslt);
        break;
    case BME68X_E_DEV_NOT_FOUND:
        ESP_LOGI("[BME680]", "API name [%s]  Error [%d] : Device not found\r\n", api_name, rslt);
        break;
    case BME68X_E_SELF_TEST:
        ESP_LOGI("[BME680]", "API name [%s]  Error [%d] : Self test error\r\n", api_name, rslt);
        break;
    case BME68X_W_NO_NEW_DATA:
        ESP_LOGI("[BME680]", "API name [%s]  Warning [%d] : No new data found\r\n", api_name, rslt);
        break;
    default:
        ESP_LOGI("[BME680]", "API name [%s]  Error [%d] : Unknown error code\r\n", api_name, rslt);
        break;
    }
}
