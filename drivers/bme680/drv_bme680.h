//
// Created by Elyass Jaoudat on 10/01/2026.
//

#ifndef LAERA_FW_DRV_BME680_H
#define LAERA_FW_DRV_BME680_H

#include "esp_err.h"
#include "driver/i2c_types.h"

#include "bme68x_defs.h"

/* ----- I2C Config ----- */
#define I2C_PORT_NUM    I2C_NUM_0
#define I2C_SDA_GPIO    21
#define I2C_SCL_GPIO    22
#define I2C_CLK_HZ      400000

/* ----- BME680 Info ----- */
#define BME680_I2C_ADDR_0           0x76
#define BME680_I2C_ADDR_1           0x77

#define BME680_REG_CHIP_ID          0xD0
#define BME680_EXPECTED_CHIP_ID     0x61

#define BME680_REG_SOFTRESET        0xE0
#define BME680_SOFTRESET_CMD        0xB6

#define BME680_REG_STATUS           0x73
#define BME680_REG_CTRL_HUM         0x72
#define BME680_REG_CTRL_MEAS        0x74
#define BME680_REG_CONFIG           0x75

/* I2C context for BME680 */
struct bme68x_i2c_ctx {
    i2c_master_bus_handle_t bus;
    i2c_master_dev_handle_t dev;
};


/* ----------------- Public Functions ----------------- */
esp_err_t bm68x_i2c_init_itf(struct bme68x_dev *handle, struct bme68x_i2c_ctx *ctx);

#endif //LAERA_FW_DRV_BME680_H