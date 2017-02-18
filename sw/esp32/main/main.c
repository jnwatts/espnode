#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <rom/rtc.h>
#include <nvs.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include <esp_event.h>
#include <esp_event_loop.h>
#include <nvs_flash.h>
#include <driver/gpio.h>
#include <string.h>

#include "app_config.h"
#include "command.h"
#include "mqtt.h"

mqtt_client_t mqtt;
int wifi_ready = false;

esp_err_t event_handler(void *ctx, system_event_t *event)
{
    if (event->event_id == SYSTEM_EVENT_STA_GOT_IP)
        wifi_ready = true;
    return ESP_OK;
}

void app_init_wifi(void)
{
    nvs_handle nvs;
    esp_err_t err;
    size_t len;
    wifi_config_t sta_config;
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    printf("Starting wifi...\n");

    ESPNODE_ERROR_CHECK(nvs_open(APP_NAMESPACE, NVS_READONLY, &nvs));
    len = sizeof(sta_config.sta.ssid);
    ESPNODE_ERROR_CHECK(nvs_get_str(nvs, WIFI_PREFIX "ssid", (char*)sta_config.sta.ssid, &len));

    len = sizeof(sta_config.sta.password);
    err = nvs_get_str(nvs, WIFI_PREFIX "password", (char*)sta_config.sta.password, &len);
    if (!(err == ESP_OK || err == ESP_ERR_NVS_NOT_FOUND))
        abort();

    len = sizeof(sta_config.sta.bssid);
    err = nvs_get_str(nvs, WIFI_PREFIX "bssid", (char*)sta_config.sta.bssid, &len);
    if (err == ESP_OK) {
        sta_config.sta.bssid_set = true;
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        sta_config.sta.bssid_set = false;
    } else {
        abort();
    }

    nvs_close(nvs);

    tcpip_adapter_init();
    ESPNODE_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    ESPNODE_ERROR_CHECK(esp_wifi_init(&cfg));
    ESPNODE_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESPNODE_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESPNODE_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    ESPNODE_ERROR_CHECK(esp_wifi_start());
    ESPNODE_ERROR_CHECK(esp_wifi_connect());

    while (!wifi_ready) {
        vTaskDelay(10 * portTICK_PERIOD_MS);
    }
}

void app_close_wifi(void)
{
    ESPNODE_ERROR_CHECK(esp_wifi_disconnect());
    ESPNODE_ERROR_CHECK(esp_wifi_stop());
    ESPNODE_ERROR_CHECK(esp_wifi_deinit());
}

void app_main(void)
{
    //TODO: Does this need re-init after deep sleep?
    ESPNODE_ERROR_CHECK(nvs_flash_init());

    //TODO: Semaphore around nvs?
    //TODO: When deep-sleep is supported, use voting mechanism to prevent deep-sleep if user starts interacting (command and/or timeout to return?)
    command_init();

    RESET_REASON reset_cause = rtc_get_reset_reason(0);
    printf("Reset cause: %02x\n", reset_cause);

    //TODO: When deep-sleep is supported, move temp reading to co-CPU and do periodic wake-then-upload
    while (true) {
        printf("Loop\n");

        //TODO: Return to accelerated SSL
        //TODO: Common code to get display client_id
        //TODO: List ssl param names
        //TODO: Module labeled prints (NIH? embedded LOG_TAG module?)
        //TODO: Manage/deal-with watchdog in ssl read-binary loop

        if (reset_cause == POWERON_RESET) {
            app_init_wifi();
        } else {
            printf("Skipped WIFI initialization due to unexpected reset\n");
        }

        if (reset_cause == POWERON_RESET) {
            ESPNODE_ERROR_CHECK(mqtt_init(&mqtt));
            ESPNODE_ERROR_CHECK(mqtt_start(&mqtt));
        } else {
            printf("Skipped MQTT initialization due to unexpected reset\n");
        }

        while (true) {
            //TODO: read temp
            //TODO: mqtt_publish(...)
            vTaskDelay(30 * 1000 * portTICK_PERIOD_MS);
        }

        // mqtt_stop(&mqtt);
        // mqtt_close(&mqtt);

        //TODO: How much of wifi can be left initialized when entering deep-sleep?
        // app_close_wifi();

        //TODO: Deep sleep
        vTaskDelay(30 * 1000 * portTICK_PERIOD_MS);
    }
}

