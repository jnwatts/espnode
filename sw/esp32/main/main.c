#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <nvs.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include <esp_event.h>
#include <esp_event_loop.h>
#include <nvs_flash.h>
#include <driver/gpio.h>
#include <string.h>

#include "command.h"

#ifdef NDEBUG
#define ESPNODE_ERROR_CHECK ESP_ERROR_CHECK
#else
#define ESPNODE_ERROR_CHECK(x) do { esp_err_t rc = (x); if (rc != ESP_OK) { printf("CHECK FAILED: %s:%d " #x "\n", __FILE__, __LINE__); while(1); } } while (0);
#endif

nvs_handle nvs;

esp_err_t event_handler(void *ctx, system_event_t *event)
{
    return ESP_OK;
}

void app_main(void)
{
    esp_err_t err;
    size_t len;

    ESPNODE_ERROR_CHECK(nvs_flash_init());
    ESPNODE_ERROR_CHECK(nvs_open("config", NVS_READWRITE, &nvs));

    //TODO: Semaphore around nvs?
    command_init();

    printf("Starting wifi...\n");

    wifi_config_t sta_config;
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    len = sizeof(sta_config.sta.ssid);
    ESPNODE_ERROR_CHECK(nvs_get_str(nvs, "ssid", (char*)sta_config.sta.ssid, &len));

    len = sizeof(sta_config.sta.password);
    err = nvs_get_str(nvs, "password", (char*)sta_config.sta.password, &len);
    if (!(err == ESP_OK || err == ESP_ERR_NVS_NOT_FOUND))
        ESPNODE_ERROR_CHECK(err);

    len = sizeof(sta_config.sta.bssid);
    err = nvs_get_str(nvs, "bssid", (char*)sta_config.sta.bssid, &len);
    if (err == ESP_OK) {
        sta_config.sta.bssid_set = true;
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        sta_config.sta.bssid_set = false;
    } else {
        ESPNODE_ERROR_CHECK(err);
    }

    tcpip_adapter_init();
    ESPNODE_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    ESPNODE_ERROR_CHECK(esp_wifi_init(&cfg));
    ESPNODE_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESPNODE_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESPNODE_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    ESPNODE_ERROR_CHECK(esp_wifi_start());
    ESPNODE_ERROR_CHECK(esp_wifi_connect());

    gpio_set_direction(GPIO_NUM_4, GPIO_MODE_OUTPUT);
    int level = 0;
    while (true) {
        gpio_set_level(GPIO_NUM_4, level);
        level = !level;
        vTaskDelay(300 / portTICK_PERIOD_MS);
    }
}

