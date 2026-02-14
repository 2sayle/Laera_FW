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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"

#include "wifi_task.h"
#include "macros.h"

/* External variables --------------------------------------------------------*/
extern EventGroupHandle_t g_wifi_event_group;

/* Exported variables --------------------------------------------------------*/
TaskHandle_t http_server_task = NULL;

/* Private variables ---------------------------------------------------------*/
static const char TAG[] = "HTTP_SERVER_TASK";

/* Private function prototypes -----------------------------------------------*/
static esp_err_t root_get_handler(httpd_req_t *req);
static esp_err_t health_get_handler(httpd_req_t *req);
static esp_err_t echo_get_handler(httpd_req_t *req);
static httpd_handle_t start_webserver(void);
static void stop_webserver(httpd_handle_t server);

/* Private functions ---------------------------------------------------------*/
static esp_err_t root_get_handler(httpd_req_t *req) {
    static const char response[] =
        "<html><body>"
        "<h1>Laera HTTP Server</h1>"
        "<p>Server is running.</p>"
        "</body></html>";

    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
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

    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &root));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &health));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &echo));

    ESP_LOGI(TAG, "HTTP server started");
    ESP_LOGI(TAG, "Handlers available: GET /, GET /health, GET /echo?msg=hello");

    return server;
}

static void stop_webserver(httpd_handle_t server) {
    if (server != NULL) {
        ESP_LOGI(TAG, "Stopping HTTP server");
        httpd_stop(server);
    }
}

/* Exported functions --------------------------------------------------------*/
void server_task(void *arg) {
    (void)arg;

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

    while (1) {
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
