/**
  ******************************************************************************
  * @file    wifi_task.c
  * @brief   Wi-Fi station task implementation.
  * @author  Elyass Jaoudat (ejaoudat@outlook.fr)
  ******************************************************************************
  * @attention
  *
  * This module initializes the Wi-Fi stack in station mode, performs a blocking
  * connection attempt, and notifies dependent tasks when the link is available.
  *
  ******************************************************************************
  */

#include "wifi_handler.h"

/* Private includes ----------------------------------------------------------*/
#include <macros.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "nvs_flash.h"

/* Private define ------------------------------------------------------------*/
/* ------ Test Config (better using menuconfig) ------ */
/* TODO */
#ifndef CONFIG_WIFI_SSID
#define CONFIG_WIFI_SSID "WIFI_SSID_REDACTED"
#endif

#ifndef CONFIG_WIFI_PASS
#define CONFIG_WIFI_PASS "WIFI_PASS_REDACTED"
#endif

/* Exported variables --------------------------------------------------------*/
EventGroupHandle_t g_wifi_event_group;

/* Private variables ---------------------------------------------------------*/
static int sRetryNum = 0;
static const int WIFI_MAX_RETRY = 10;

static char* TAG = "WIFI HANDLER";

/**
  * @brief  Handles Wi-Fi and IP events.
  * @param  arg User context pointer (unused).
  * @param  event_base Event base identifying the producer.
  * @param  event_id Event identifier within the base.
  * @param  event_data Pointer to event-specific data.
  * @retval None
  */
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {

    UNUSED(arg); UNUSED(event_data);

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WIFI STA START");
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (sRetryNum < WIFI_MAX_RETRY) {
            sRetryNum++;
            ESP_LOGI(TAG, "WIFI STA DISCONNECTED --> Retrying... %d/%d", sRetryNum, WIFI_MAX_RETRY);
            esp_wifi_connect();
        } else {
            ESP_LOGI(TAG, "WIFI STA DISCONNECTED --> Exiting...");
            xEventGroupSetBits(g_wifi_event_group, WIFI_FAIL_BIT);
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        sRetryNum = 0;
        ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(g_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/**
  * @brief  Initializes STA mode and waits for connection result.
  * @param  None
  * @retval ESP_OK           Connected and IP obtained.
  * @retval ESP_FAIL         Connection failed after retries.
  * @retval ESP_ERR_NO_MEM   Event group allocation failed.
  * @retval ESP_ERR_TIMEOUT  Connection timed out.
  */
static esp_err_t wifi_connect_sta_blocking(void) {
    g_wifi_event_group = xEventGroupCreate();
    if (!g_wifi_event_group)
        return ESP_ERR_NO_MEM;

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Create default STA interface */
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Register the handlers */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, CONFIG_WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, CONFIG_WIFI_PASS, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    /* Wait for connection */
    EventBits_t bits = xEventGroupWaitBits(g_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(30000) // 30 seconds
   );

    if (bits & WIFI_CONNECTED_BIT) return ESP_OK;
    if (bits & WIFI_FAIL_BIT) return ESP_FAIL;
    else return ESP_ERR_TIMEOUT;

}

/**
  * @brief  FreeRTOS task entry point for Wi-Fi initialization and connection.
  * @param  arg Task argument (unused).
  * @retval None
  */
void wifi_task(void *arg) {

    UNUSED(arg);
    ESP_LOGI(TAG, "Starting WIFI STA");

    /* NVS is mandatory for WiFi */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ret = wifi_connect_sta_blocking();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect to WiFi");
        vTaskDelete(NULL);
    }

    ESP_LOGI(TAG, "WIFI STA CONNECTED");
    xTaskNotifyGiveIndexed(http_server_task, 0)

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

}
