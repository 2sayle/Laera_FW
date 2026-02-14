/**
  ******************************************************************************
  * @file    server_task.c
  * @brief   HTTP server task implementation.
  ******************************************************************************
  * @attention
  *
  * This module waits until Wi-Fi station connectivity is confirmed, starts a
  * lightweight HTTP server, and registers a few example URI handlers.
  *
  ******************************************************************************
  */

/* Private includes ----------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_spiffs.h"

#include "wifi_task.h"
#include "sensor_task.h"
#include "macros.h"

/* External variables --------------------------------------------------------*/
extern EventGroupHandle_t g_wifi_event_group;

extern const unsigned char page_html_start[] asm("_binary_page_html_start");
extern const unsigned char page_html_end[] asm("_binary_page_html_end");

/* Exported variables --------------------------------------------------------*/
TaskHandle_t http_server_task = NULL;

/* Private variables ---------------------------------------------------------*/
static const char TAG[] = "HTTP_SERVER_TASK";
#define HISTORY_BASE_PATH "/spiffs"
#define HISTORY_FILE_PATH HISTORY_BASE_PATH "/sensor_history.csv"
#define HISTORY_TMP_PATH  HISTORY_BASE_PATH "/sensor_history.tmp"
#define HISTORY_RETENTION_MS (24UL * 60UL * 60UL * 1000UL)
#define HISTORY_REBOOT_GUARD_MS 60000UL
#define HISTORY_COMPACT_EVERY_SAMPLES 30U

typedef struct {
    sensor_data_t sample;
    bool has_data;
    TickType_t last_tick;
    uint32_t sample_timestamp_ms;
} latest_sensor_cache_t;

static latest_sensor_cache_t g_sensor_cache = {0};
static portMUX_TYPE g_sensor_cache_mux = portMUX_INITIALIZER_UNLOCKED;
static bool g_history_fs_ready = false;
static uint32_t g_history_samples_since_compact = 0U;

/* Private function prototypes -----------------------------------------------*/
static esp_err_t root_get_handler(httpd_req_t *req);
static esp_err_t health_get_handler(httpd_req_t *req);
static esp_err_t echo_get_handler(httpd_req_t *req);
static esp_err_t sensor_get_handler(httpd_req_t *req);
static esp_err_t sensor_history_csv_get_handler(httpd_req_t *req);
static httpd_handle_t start_webserver(void);
static void stop_webserver(httpd_handle_t server);
static void history_init_storage(void);
static void history_append_sample(const sensor_data_t *sample, uint32_t timestamp_ms);
static void history_compact(uint32_t now_ms);
static uint32_t history_read_last_timestamp(void);

/* Private functions ---------------------------------------------------------*/

static esp_err_t root_get_handler(httpd_req_t *req) {
    const size_t page_size = page_html_end - page_html_start;
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, (const char *)page_html_start, page_size);
}

static esp_err_t health_get_handler(httpd_req_t *req) {
    static const char response[] = "{\"status\":\"ok\",\"service\":\"laera-fw\"}";

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t echo_get_handler(httpd_req_t *req) {
    char query[128] = {0};
    char message[96] = "";

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        if (httpd_query_key_value(query, "msg", message, sizeof(message)) != ESP_OK) {
            strncpy(message, "(missing msg parameter)", sizeof(message) - 1U);
        }
    } else {
        strncpy(message, "(no query string)", sizeof(message) - 1U);
    }

    char response[160] = {0};
    (void)snprintf(response, sizeof(response), "{\"echo\":\"%s\"}", message);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t sensor_get_handler(httpd_req_t *req) {
    latest_sensor_cache_t cache = {0};
    taskENTER_CRITICAL(&g_sensor_cache_mux);
    cache = g_sensor_cache;
    taskEXIT_CRITICAL(&g_sensor_cache_mux);

    const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    uint32_t age_ms = 0U;
    if (cache.has_data) {
        age_ms = (uint32_t)((xTaskGetTickCount() - cache.last_tick) * portTICK_PERIOD_MS);
    }

    char response[256] = {0};
    (void)snprintf(
        response,
        sizeof(response),
        "{\"status\":\"ok\",\"online\":true,\"has_data\":%s,\"sample_age_ms\":%lu,"
        "\"timestamp_ms\":%lu,\"temperature\":%.2f,\"humidity\":%.2f,"
        "\"pressure_hpa\":%.2f,\"gas_resistance\":%.2f}",
        cache.has_data ? "true" : "false",
        (unsigned long)age_ms,
        (unsigned long)(cache.has_data ? cache.sample_timestamp_ms : now_ms),
        cache.sample.temperature,
        cache.sample.humidity,
        cache.sample.pressure / 100.0f,
        cache.sample.gas_resistance);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t sensor_history_csv_get_handler(httpd_req_t *req) {
    static const char csv_header[] =
        "timestamp_ms,temperature,humidity,pressure_hpa,gas_resistance\n";
    httpd_resp_set_type(req, "text/csv");

    if (httpd_resp_send_chunk(req, csv_header, HTTPD_RESP_USE_STRLEN) != ESP_OK) {
        httpd_resp_sendstr_chunk(req, NULL);
        return ESP_FAIL;
    }

    if (!g_history_fs_ready) {
        httpd_resp_sendstr_chunk(req, NULL);
        return ESP_OK;
    }

    FILE *fp = fopen(HISTORY_FILE_PATH, "r");
    if (fp == NULL) {
        httpd_resp_sendstr_chunk(req, NULL);
        return ESP_OK;
    }

    char line[160] = {0};
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (httpd_resp_send_chunk(req, line, HTTPD_RESP_USE_STRLEN) != ESP_OK) {
            fclose(fp);
            httpd_resp_sendstr_chunk(req, NULL);
            return ESP_FAIL;
        }
    }

    fclose(fp);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

static httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
        return NULL;
    }

    static const httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get_handler,
        .user_ctx = NULL,
    };
    static const httpd_uri_t health = {
        .uri = "/health",
        .method = HTTP_GET,
        .handler = health_get_handler,
        .user_ctx = NULL,
    };
    static const httpd_uri_t echo = {
        .uri = "/echo",
        .method = HTTP_GET,
        .handler = echo_get_handler,
        .user_ctx = NULL,
    };
    static const httpd_uri_t sensor = {
        .uri = "/api/sensor",
        .method = HTTP_GET,
        .handler = sensor_get_handler,
        .user_ctx = NULL,
    };
    static const httpd_uri_t sensor_history_csv = {
        .uri = "/api/sensor/history.csv",
        .method = HTTP_GET,
        .handler = sensor_history_csv_get_handler,
        .user_ctx = NULL,
    };

    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &root));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &health));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &echo));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &sensor));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &sensor_history_csv));

    ESP_LOGI(TAG, "HTTP server started");
    ESP_LOGI(TAG, "Handlers: GET /, GET /health, GET /echo?msg=hello, GET /api/sensor, GET /api/sensor/history.csv");

    return server;
}

static void stop_webserver(httpd_handle_t server) {
    if (server != NULL) {
        ESP_LOGI(TAG, "Stopping HTTP server");
        httpd_stop(server);
    }
}

static void history_init_storage(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = HISTORY_BASE_PATH,
        .partition_label = "storage",
        .max_files = 4,
        .format_if_mount_failed = true,
    };

    const esp_err_t err = esp_vfs_spiffs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "SPIFFS mount failed (%s). CSV history disabled.", esp_err_to_name(err));
        g_history_fs_ready = false;
        return;
    }

    size_t total = 0;
    size_t used = 0;
    if (esp_spiffs_info(conf.partition_label, &total, &used) == ESP_OK) {
        ESP_LOGI(TAG, "SPIFFS mounted: total=%u, used=%u bytes", (unsigned)total, (unsigned)used);
    }

    g_history_fs_ready = true;
    g_history_samples_since_compact = 0U;

    const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    const uint32_t last_ts = history_read_last_timestamp();
    if ((last_ts > 0U) && (last_ts > (now_ms + HISTORY_REBOOT_GUARD_MS))) {
        ESP_LOGW(TAG, "Detected stale CSV history from a previous boot. Resetting history file.");
        FILE *fp = fopen(HISTORY_FILE_PATH, "w");
        if (fp != NULL) {
            fclose(fp);
        }
    }

    history_compact(now_ms);
}

static uint32_t history_read_last_timestamp(void) {
    FILE *fp = fopen(HISTORY_FILE_PATH, "r");
    if (fp == NULL) {
        return 0U;
    }

    char line[160] = {0};
    uint32_t last_ts = 0U;
    while (fgets(line, sizeof(line), fp) != NULL) {
        char *endptr = NULL;
        errno = 0;
        const unsigned long ts = strtoul(line, &endptr, 10);
        if ((errno == 0) && (endptr != line) && (*endptr == ',')) {
            last_ts = (uint32_t)ts;
        }
    }

    fclose(fp);
    return last_ts;
}

static void history_compact(uint32_t now_ms) {
    if (!g_history_fs_ready) {
        return;
    }

    FILE *src = fopen(HISTORY_FILE_PATH, "r");
    if (src == NULL) {
        return;
    }

    FILE *dst = fopen(HISTORY_TMP_PATH, "w");
    if (dst == NULL) {
        fclose(src);
        return;
    }

    const uint32_t cutoff_ms = (now_ms > HISTORY_RETENTION_MS) ? (now_ms - HISTORY_RETENTION_MS) : 0U;
    char line[160] = {0};
    while (fgets(line, sizeof(line), src) != NULL) {
        char *endptr = NULL;
        errno = 0;
        const unsigned long ts = strtoul(line, &endptr, 10);
        if ((errno != 0) || (endptr == line) || (*endptr != ',')) {
            continue;
        }

        const uint32_t row_ts = (uint32_t)ts;
        if ((row_ts >= cutoff_ms) && (row_ts <= (now_ms + HISTORY_REBOOT_GUARD_MS))) {
            fputs(line, dst);
        }
    }

    fclose(src);
    fclose(dst);
    (void)remove(HISTORY_FILE_PATH);
    (void)rename(HISTORY_TMP_PATH, HISTORY_FILE_PATH);
}

static void history_append_sample(const sensor_data_t *sample, uint32_t timestamp_ms) {
    if ((!g_history_fs_ready) || (sample == NULL)) {
        return;
    }

    FILE *fp = fopen(HISTORY_FILE_PATH, "a");
    if (fp == NULL) {
        return;
    }

    (void)fprintf(
        fp,
        "%lu,%.2f,%.2f,%.2f,%.2f\n",
        (unsigned long)timestamp_ms,
        sample->temperature,
        sample->humidity,
        sample->pressure / 100.0f,
        sample->gas_resistance);
    fclose(fp);

    g_history_samples_since_compact++;
    if (g_history_samples_since_compact >= HISTORY_COMPACT_EVERY_SAMPLES) {
        g_history_samples_since_compact = 0U;
        history_compact(timestamp_ms);
    }
}

/* Exported functions --------------------------------------------------------*/

/**
 * @brief FreeRTOS task entry point for HTTP server management. Waits for Wi-Fi connectivity,
 * starts the HTTP server, and updates the latest sensor data cache for API responses.
 * @param arg 
 */
void server_task(void *arg) {
    QueueHandle_t sensor_queue = (QueueHandle_t)arg;
    if (sensor_queue == NULL) {
        ESP_LOGE(TAG, "Sensor queue is NULL");
        vTaskDelete(NULL);
        return;
    }

    http_server_task = xTaskGetCurrentTaskHandle();

    if (g_wifi_event_group == NULL) {
        ESP_LOGE(TAG, "Wi-Fi event group is not initialized");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Waiting for Wi-Fi connectivity before starting HTTP server");

    EventBits_t bits = xEventGroupWaitBits(
        g_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        portMAX_DELAY);

    if ((bits & WIFI_CONNECTED_BIT) == 0U) {
        ESP_LOGE(TAG, "Wi-Fi connection not available, HTTP server not started");
        vTaskDelete(NULL);
        return;
    }

    httpd_handle_t server = start_webserver();
    if (server == NULL) {
        vTaskDelete(NULL);
        return;
    }
    history_init_storage();

    while (1) {
        sensor_data_t sample = {0};
        while (xQueueReceive(sensor_queue, &sample, 0) == pdPASS) {
            const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
            taskENTER_CRITICAL(&g_sensor_cache_mux);
            g_sensor_cache.sample = sample;
            g_sensor_cache.has_data = true;
            g_sensor_cache.last_tick = xTaskGetTickCount();
            g_sensor_cache.sample_timestamp_ms = now_ms;
            taskEXIT_CRITICAL(&g_sensor_cache_mux);

            history_append_sample(&sample, now_ms);
        }

        EventBits_t current_bits = xEventGroupGetBits(g_wifi_event_group);
        if ((current_bits & WIFI_CONNECTED_BIT) == 0U) {
            ESP_LOGW(TAG, "Wi-Fi disconnected, stopping HTTP server task");
            stop_webserver(server);
            vTaskDelete(NULL);
            return;
        }


        LOG_AVAILABLE_STACK(TAG);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
