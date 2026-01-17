/* macro.h
 *
 * Small, practical macros for ESP-IDF + FreeRTOS projects.
 * Goal: reduce boilerplate without hiding bugs.
 *
 * Usage philosophy:
 *  - Blocking delays are for short HW timings only (us-level).
 *  - Task delays are for scheduling (ms+).
 *  - Always make errors loud and early.
 *
 *  Author: Elyass Jaoudat
 *  Date: 10/10/2026
 *
 */

#pragma once

#ifndef LAERA_FW_MACROS_H
#define LAERA_FW_MACROS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "sdkconfig.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a)   (sizeof(a) / sizeof((a)[0]))
#endif

#ifndef MIN
#define MIN(a,b)        (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b)        (((a) > (b)) ? (a) : (b))
#endif

#ifndef CLAMP
#define CLAMP(x, lo, hi) ( ((x) < (lo)) ? (lo) : (((x) > (hi)) ? (hi) : (x)) )
#endif

#ifndef BIT
#define BIT(n)          (1UL << (n))
#endif

#ifndef BIT64
#define BIT64(n)        (1ULL << (n))
#endif

#ifndef UNUSED
#define UNUSED(x)       ((void)(x))
#endif

/* ----------------------------- Delays ---------------------------------- */
/* Blocking delay: for short, precise hardware waits. */
#define BLOCKING_DELAY_US(us)     do { esp_rom_delay_us((uint32_t)(us)); } while (0)

/* Strong suggestion: do NOT use blocking ms delays in tasks.
 * If you REALLY need it for a datasheet timing, keep name explicit. */
#define BLOCKING_DELAY_MS(ms)     do { esp_rom_delay_us((uint32_t)(ms) * 1000U); } while (0)

/* Task delay: yields to scheduler. Must be called when scheduler is running. */
#define TASK_DELAY_MS(ms)                                                          \
    do {                                                                           \
        configASSERT(xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED);       \
        vTaskDelay(pdMS_TO_TICKS((ms)));                                           \
    } while (0)

/* Stable periodic loop helper (reduces drift vs vTaskDelay in a loop). */
#define TASK_DELAY_UNTIL_MS(last_wake_tick, period_ms)                             \
    do {                                                                           \
        configASSERT(xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED);       \
        vTaskDelayUntil(&(last_wake_tick), pdMS_TO_TICKS((period_ms)));            \
    } while (0)

/* --------------------------- Error helpers ------------------------------ */
/* Return if ESP-IDF call fails */
#define ESP_RETURN_ON_ERROR(expr, tag, fmt, ...)                                   \
    do {                                                                           \
        esp_err_t __err = (expr);                                                  \
        if (__err != ESP_OK) {                                                     \
            ESP_LOGE((tag), "%s failed: %s | " fmt, #expr, esp_err_to_name(__err), \
                     ##__VA_ARGS__);                                               \
            return __err;                                                          \
        }                                                                          \
    } while (0)

/* Return void if ESP-IDF call fails */
#define ESP_RETURN_VOID_ON_ERROR(expr, tag, fmt, ...)                              \
    do {                                                                           \
        esp_err_t __err = (expr);                                                  \
        if (__err != ESP_OK) {                                                     \
            ESP_LOGE((tag), "%s failed: %s | " fmt, #expr, esp_err_to_name(__err), \
                     ##__VA_ARGS__);                                               \
            return;                                                                \
        }                                                                          \
    } while (0)

/* Log but keep going */
#define ESP_LOG_ON_ERROR(expr, tag, fmt, ...)                                      \
    do {                                                                           \
        esp_err_t __err = (expr);                                                  \
        if (__err != ESP_OK) {                                                     \
            ESP_LOGE((tag), "%s failed: %s | " fmt, #expr, esp_err_to_name(__err), \
                     ##__VA_ARGS__);                                               \
        }                                                                          \
    } while (0)

/* ---------------------------- Assertions -------------------------------- */
#ifndef ASSERT
#define ASSERT(cond)          do { configASSERT((cond)); } while (0)
#endif

#define ASSERT_NOT_NULL(p)    do { configASSERT((p) != NULL); } while (0)

/* ---------------------------- Memory ------------------------------------ */
#define MALLOC_OR_RETURN(ptr, size)                                                \
    do {                                                                           \
        (ptr) = pvPortMalloc((size));                                              \
        if ((ptr) == NULL) {                                                       \
            return ESP_ERR_NO_MEM;                                                 \
        }                                                                          \
    } while (0)

#define CALLOC_OR_RETURN(ptr, count, size_each)                                    \
    do {                                                                           \
        size_t __sz = (size_t)(count) * (size_t)(size_each);                       \
        (ptr) = pvPortMalloc(__sz);                                                \
        if ((ptr) == NULL) {                                                       \
            return ESP_ERR_NO_MEM;                                                 \
        }                                                                          \
        memset((ptr), 0, __sz);                                                    \
    } while (0)

#define FREE_AND_NULL(p)      do { if ((p) != NULL) { vPortFree((p)); (p) = NULL; } } while (0)

/* --------------------------- Compile-time -------------------------------- */
#define STATIC_ASSERT(cond, msg) _Static_assert((cond), msg)

/* ------------------------------ Logging ---------------------------------- */
/* Optional: set local TAG quickly */
#define TAG_THIS(name) static const char *TAG = (name)

/* ------------------------------ Utilities -------------------------------- */
/* Convert Hz to period in ms (integer, rounded) */
#define HZ_TO_MS(hz)    ( (hz) ? (1000U / (uint32_t)(hz)) : 0U )

/* Safe integer rounding divide */
#define DIV_ROUND_UP(n, d) ( ((n) + (d) - 1U) / (d) )

#ifdef __cplusplus
}
#endif

#endif //LAERA_FW_MACROS_H