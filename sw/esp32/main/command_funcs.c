#include <nvs.h>
#include <stdio.h>
#include <string.h>
#include <microrl.h>
#include <stdlib.h>
#include "command.h"

extern nvs_handle nvs;

static int command_wifi(int argc, const char * const * argv)
{
    esp_err_t err;
    int i;
    size_t len;
    char buf[64+1];
    char param[10];
    char *query_pos;

    if (argc < 2) {
wifi_usage:
        printf("Usage: %s (ssid|bssid|password)[?] [<value>]\n", argv[0]);
        return 1;
    }

    strncpy(param, argv[1], sizeof(param));
    query_pos = strchr(param, '?');
    if (query_pos)
        *query_pos = '\0';

    if (!(
        strcmp(param, "ssid") == 0 ||
        strcmp(param, "bssid") == 0 ||
        strcmp(param, "password") == 0
        )) {
        printf("Invalid param %s\n", param);
        goto wifi_usage;
    }

    if (query_pos) {
        len = sizeof(buf);
        err = nvs_get_str(nvs, param, buf, &len);

        if (err == ESP_OK) {
            if (strcmp(param, "password") == 0)
                strcpy(buf, "is set");
            printf("%s: %s\n", param, buf);
        } else if (err != ESP_OK) {
            printf("Failed to read param \"%s\": %d\n", param, err);
            return 1;
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
            return 1;
        }
    }

    return 0;
}

static int command_xset(int argc, const char * const * argv)
{
    // printf("\nReady: Send hex data, terminate with newline or carriage return.\n");
    // int c;

    // uint8_t *buf = malloc(4096);

    // while ((c = getchar()) >= 0) {
    //     switch (c) {
    //     case KEY_LF:
    //     case KEY_CR:
    //         break;
    //     }
    // }
    return 0;
}

static int command_commit(int argc, const char * const * argv)
{
    nvs_commit(nvs);

    return 0;
}

static int command_clear(int argc, const char * const * argv)
{
    nvs_erase_all(nvs);

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
        "  wifi <param> <value> -- Set wifi <param> (one of ssid, bssid, password) to <value>\n"
        "  wifi <param>?        -- Read wifi <param>\n"
        "  ca-cert              -- Set <key> to binary value represented as hex, terminated by newline\n"
        "  clear <key>          -- Delete <key>\n"
        "  commit               -- Save changes\n"
        "  list                 -- List keys and their lengths\n"
        "  help                 -- Show this help screen\n"
        );
    return 0;
}

command_t commands[] = {
    { command_wifi, "wifi" },
    { command_xset, "xset" },
    { command_clear, "clear" },
    { command_commit, "commit" },
    { command_echo, "echo" },
    { command_help, "help" },
    { NULL, NULL }
};