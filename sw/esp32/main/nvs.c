#include <esp_err.h>
#include <nvs_flash.h>
#include <stdlib.h>
#include <stdio.h>
#include "app_config.h"

esp_err_t nvs_get_str_static(nvs_handle nvs, const char *param, char *buffer, size_t len)
{
    return nvs_get_str(nvs, param, buffer, &len);
}

esp_err_t nvs_get_str_heap(nvs_handle nvs, const char *param, char **buffer)
{
    esp_err_t err;
    size_t len = 0;

    *buffer = NULL;
    err = nvs_get_str(nvs, param, NULL, &len);
    if (err == ESP_OK && len) {
        *buffer = malloc(len);
        err = nvs_get_str(nvs, param, *buffer, &len);
    }
    return err;
}