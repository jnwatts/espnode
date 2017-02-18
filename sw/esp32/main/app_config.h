#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <esp_err.h>
#include <nvs.h>

#ifdef NDEBUG
#define ESPNODE_ERROR_CHECK ESP_ERROR_CHECK
#else
#define ESPNODE_ERROR_CHECK(x) do { esp_err_t rc = (x); if (rc != ESP_OK) { printf("CHECK FAILED: %s:%d " #x "\n", __FILE__, __LINE__); while(1) { vTaskDelay(100 * portTICK_PERIOD_MS); } } } while (0);
#endif

#define APP_NAMESPACE "config"

#define WIFI_PREFIX "wifi."
#define MQTT_PREFIX "mqtt."
#define SSL_PREFIX "ssl."

esp_err_t nvs_get_str_static(nvs_handle nvs, const char *param, char *buffer, size_t len);
esp_err_t nvs_get_str_heap(nvs_handle nvs, const char *param, char **buffer);

#endif