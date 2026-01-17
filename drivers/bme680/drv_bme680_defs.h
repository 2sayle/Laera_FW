//
// Created by Elyass Jaoudat on 10/01/2026.
//


#ifndef LAERA_FW_DRV_BME680_DEFS_H
#define LAERA_FW_DRV_BME680_DEFS_H


/* Custom structure */
typedef struct
{
#ifdef BME68X_USE_FPU
    float temperature_c;
    float pressure_pa;
    float humidity_pct;
    float gas_res_ohm;
#elif defined(BME68X_USE_INT)
    /* If the library is built without FPU, bme68x_data uses integer scaled units. */
    int16_t temperature_c_x100;   /* °C * 100 */
    uint32_t pressure_pa;         /* Pa */
    uint32_t humidity_pct_x1000;  /* %RH * 1000 */
    uint32_t gas_res_ohm;         /* Ohm */
#endif
} bme680_reading_t;



#endif //LAERA_FW_DRV_BME680_DEFS_H