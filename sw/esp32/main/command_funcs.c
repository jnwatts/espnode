#include <nvs.h>
#include <stdio.h>
#include <string.h>
#include <microrl.h>
#include <stdlib.h>
#include "app_config.h"
#include "command.h"

nvs_handle nvs;
int is_nvs_open = false;

esp_err_t open_config(void) {
    esp_err_t err;
    if (!is_nvs_open) {
        is_nvs_open = true;
        err = nvs_open("config", NVS_READWRITE, &nvs);
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
            printf("Warning: Truncated value to %d characters\n", len - 1);
            len = sizeof(buf);
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

    if (strcmp(param, "endpoint") == 0) {
        return ESP_OK;
    } else if (strcmp(param, "port") == 0) {
        return ESP_OK;
    } else if (strcmp(param, "username") == 0) {
        if (read_only)
            *read_only = true;
        return ESP_OK;
    } else if (strcmp(param, "password") == 0) {
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
        "  ca-cert                -- Set <key> to binary value represented as hex, terminated by newline\n"
        "  clear                  -- Delete all params"
        "  list                   -- List keys and their lengths\n"
        "  help                   -- Show this help screen\n"
        );
    return 0;
}

command_t commands[] = {
    { command_wifi, "wifi" },
    { command_mqtt, "mqtt" },
    { command_clear, "clear" },
    { command_echo, "echo" },
    { command_help, "help" },
    { NULL, NULL }
};