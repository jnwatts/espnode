#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_task_wdt.h>
#include <soc/uart_struct.h>
#include <nvs.h>
#include <stdio.h>
#include <string.h>
#include <microrl.h>
#include <stdlib.h>
#include "app_config.h"
#include "command.h"
#include "mqtt.h"

nvs_handle nvs;
int is_nvs_open = false;

esp_err_t open_config(void) {
    esp_err_t err;
    if (!is_nvs_open) {
        is_nvs_open = true;
        err = nvs_open(APP_NAMESPACE, NVS_READWRITE, &nvs);
        if (err != ESP_OK)
            printf("Failed to open NVS: %d\n", err);
        return err;
    } else {
        return ESP_OK;
    }
}

void close_config(void) {
    if (is_nvs_open) {
        is_nvs_open = false;
        nvs_close(nvs);
    }
}

/**
 * Callback for determining parameter validity
 * \param[in] param Parameter name
 * \param[out] read_only May be NULL. Set to true if parameter is read-only
 * \return ESP_OK is parameter exists, ESP_ERR_INVALID_ARG otherwise
 */
typedef esp_err_t (*param_check_t)(const char *param, int *read_only);

/**
 * Helper function for setting params
 * \param[in] argc Argument count
 * \param[in] argv Argument array
 * \param[in] prefix String to prefix to params
 * \param[in] param_check (Optional) See param_check_t
 */
static int command_param(int argc, const char * const * argv, const char *prefix, param_check_t param_check)
{
    esp_err_t err;
    int i;
    size_t len;
    char buf[64+1];
    char param[32];
    char *query_pos;
    int param_read_only;

    if (argc < 2) {
param_usage:
        printf("Usage: %s <param>[?] [<value>]\n", argv[0]);
        return 1;
    }

    strncpy(param, prefix, sizeof(param));
    strncat(param, argv[1], sizeof(param) - strlen(prefix));
    query_pos = strchr(param, '?');
    if (query_pos)
        *query_pos = '\0';

    if (param_check) {
        err = param_check(param, &param_read_only);
        if (err != ESP_OK) {
            printf("Invalid param %s\n", param);
            goto param_usage;
        }
    } else {
        param_read_only = false;
    }

    err = open_config();
    if (err != ESP_OK)
        return 1;

    if (query_pos) {
        len = sizeof(buf);
        err = nvs_get_str(nvs, param, buf, &len);

        if (err == ESP_OK) {
            if (param_read_only)
                strcpy(buf, "is set");
            printf("%s: %s\n", param, buf);
        } else if (err != ESP_OK) {
            printf("Failed to read param \"%s\": %d\n", param, err);
            goto out;
        }
    } else {
        len = 0;
        for (i = 2; i < argc && len < sizeof(buf); ++i) {
            int l = strlen(argv[i]);
            int remaining = sizeof(buf) - len;
            if (l > remaining)
                l = remaining;
            strncpy(buf + len, argv[i], l);
            len += l;
        }
        if (len >= sizeof(buf)) {
            len = sizeof(buf) - 1;
            printf("Warning: Truncated value to %d characters\n", len);
        }
        buf[len] = '\0';

        if (len == 0)
            err = nvs_erase_key(nvs, param);
        else
            err = nvs_set_str(nvs, param, buf);

        if (err == ESP_OK) {
            printf("%s: %s\n", param, (len == 0 ? "cleared" : buf));
        } else if (err != ESP_OK) {
            printf("Failed to %s param \"%s\": %d\n", (len == 0 ? "clear" : "set"), param, err);
            goto out;
        }
    }

out:
    nvs_commit(nvs);
    close_config();
    if (err != ESP_OK)
        return 1;
    return 0;
}

esp_err_t command_wifi_param_check(const char *param, int *read_only)
{
    if (read_only)
        *read_only = false;

    if (strcmp(param, WIFI_PREFIX "ssid") == 0) {
        return ESP_OK;
    } else if (strcmp(param, WIFI_PREFIX "bssid") == 0) {
        return ESP_OK;
    } else if (strcmp(param, WIFI_PREFIX "password") == 0) {
        if (read_only)
            *read_only = true;
        return ESP_OK;
    }

    return ESP_ERR_INVALID_ARG;
}

static int command_wifi(int argc, const char * const * argv)
{
    return command_param(argc, argv, WIFI_PREFIX, command_wifi_param_check);
}

esp_err_t command_mqtt_param_check(const char *param, int *read_only)
{
    if (read_only)
        *read_only = false;

    if (strcmp(param, MQTT_PREFIX "hostname") == 0) {
        return ESP_OK;
    } else if (strcmp(param, MQTT_PREFIX "port") == 0) {
        return ESP_OK;
    } else if (strcmp(param, MQTT_PREFIX "username") == 0) {
        if (read_only)
            *read_only = true;
        return ESP_OK;
    } else if (strcmp(param, MQTT_PREFIX "password") == 0) {
        if (read_only)
            *read_only = true;
        return ESP_OK;
    }

    return ESP_ERR_INVALID_ARG;
}

static int command_mqtt(int argc, const char * const * argv)
{
    return command_param(argc, argv, MQTT_PREFIX, command_mqtt_param_check);
}

esp_err_t command_ssl_param_check(const char *param, int *read_only)
{
    if (read_only)
        *read_only = true;

    if (strcmp(param, SSL_PREFIX "ca_cert") == 0) {
        return ESP_OK;
    } else if (strcmp(param, SSL_PREFIX "client_cert") == 0) {
        return ESP_OK;
    } else if (strcmp(param, SSL_PREFIX "client_key") == 0) {
        return ESP_OK;
    }

    return ESP_ERR_INVALID_ARG;
}

static int command_ssl(int argc, const char * const * argv)
{
    size_t buflen = 0x2000;
    char *buf = malloc(buflen); // Maximum size is between 0x1000 and 0x2000: Only allocates 1 additional page if current is full (Maybe need to consider FAT?)
    int len = 0;
    int c;
    bool digit = false;
    char param[32];
    bool waiting = true;

    if (argc < 2) {
param_usage:
        printf("Usage: %s <param>\n", argv[0]);
        return 1;
    }

    strcpy(param, SSL_PREFIX);
    strncat(param, argv[1], sizeof(param) - strlen(SSL_PREFIX));
    if (command_ssl_param_check(param, NULL) != ESP_OK) {
        printf("Invalid param %s\n", param);
        goto param_usage;
    }

    printf("Ready: send hex string terminated by newline\n");
    vTaskPrioritySet(NULL, tskIDLE_PRIORITY+10);
    esp_task_wdt_feed();
    esp_task_wdt_delete();
    for (;;) {
        if (UART0.status.rxfifo_cnt == 0) {
            taskYIELD();
            if (!waiting) {
                printf(".");
                waiting = true;
            }
            continue;
        } else if (UART0.status.rxfifo_cnt >= 127) {
            printf("Failure :-(\n");
        }
        waiting = false;
        c = UART0.fifo.rw_byte;

        if (c <= 0)
            continue;

        if (c == KEY_LF || c == KEY_CR)
            break;

        if (c >= 0x30 && c <= 0x39) {
            c &= 0x0f;
        } else if (c >= 0x41 && c <= 0x46) {
            c -= 0x37;
        } else if (c >= 0x61 && c <= 0x66) {
            c -= 0x57;
        } else if (c == ' ') {
            continue;
        } else {
#if 0
            printf("Invalid character %d '%c'\n", c, c);
            goto err;
#else
            continue;
#endif
        }

        if (!digit) {
            buf[len] = c << 4;
        } else {
            buf[len++] |= c;
        }
        digit = !digit;

        if (len >= buflen) {
            len = buflen - 1;
            break;
        }
    }
    vTaskPrioritySet(NULL, tskIDLE_PRIORITY);
    esp_task_wdt_init();
    if (digit) {
        printf("Uneven digits\n");
        goto err;
    }
    buf[len] = '\0';
    printf("\n%s\n", buf);

    if (open_config() != ESP_OK)
        return 1;
    nvs_set_str(nvs, param, buf);
    nvs_commit(nvs);
    close_config();

    printf("%s: %d bytes\n", param, len);;

    return 0;
err:
    printf("Failed to parse hex input");
    free(buf);
    return 1;
}

static int command_client_id(int argc, const char * const * argv)
{
    esp_err_t err;
    char client_id[MQTT_CLIENT_ID_LEN];

    err = mqtt_client_id(client_id);
    if (err != ESP_OK) {
        printf("Failed to read client-id");
        return 1;
    }

    printf("client-id: %s\n", client_id);

    return 0;
}

static int command_clear(int argc, const char * const * argv)
{
    open_config();
    nvs_erase_all(nvs);
    close_config();

    return 0;
}

int command_echo(int argc, const char * const * argv)
{
    int i;
    for (i = 0; i < argc; ++i) {
        printf("arg[%d]: %s\n", i, argv[i]);
    }
    return 0;
}

int command_help(int argc, const char * const * argv)
{
    (void)argc;
    (void)argv;

    printf("Available commands:\n"
           "  wifi <param> [<value>] -- Set wifi <param> (one of ssid, bssid, password) to <value>, use empty string to clear\n"
           "  wifi <param>?          -- Read wifi <param>\n"
           "  mqtt <param> [<value>] -- Set mqtt <param> (one of endpoint, port, username, password) to <value>\n"
           "  mqtt <param>?          -- Read mqtt <param>\n"
           "  ssl <param>            -- Set ssl <param> (one of ca_cert, client_cert, client_key) to binary value represented as hex, terminated by newline\n"
           "  client_id              -- Print MQTT client-id\n"
           "  clear                  -- Delete all params\n"
           "  list                   -- List keys and their lengths\n"
           "  help                   -- Show this help screen\n"
          );
    return 0;
}

command_t commands[] = {
    { command_wifi, "wifi" },
    { command_mqtt, "mqtt" },
    { command_ssl, "ssl" },
    { command_client_id, "client_id" },
    { command_clear, "clear" },
    { command_echo, "echo" },
    { command_help, "help" },
    { NULL, NULL }
};