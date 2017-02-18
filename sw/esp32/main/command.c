#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>
#include <string.h>
#include <microrl.h>

#include "command.h"

#define TASK_STACK_SIZE 10240
#define TASK_PRIORITY tskIDLE_PRIORITY

static TaskHandle_t xCommandTask;
microrl_t rl;

extern command_t commands[];

static void command_print(const char * str)
{
    printf("%s", str);
}

static int command_exec(int argc, const char * const * argv)
{
    command_t *c;

    if (argc < 1)
        return 0;

    for (c = &commands[0]; c != NULL && c->func != NULL && c->argv0 != NULL; c++) {
        if (strcmp(argv[0], c->argv0) == 0)
            return c->func(argc, argv);
    }

    printf("Invalid command: %s\n", argv[0]);
    command_help(argc, argv);
    return 1;
}

static void command_task(void *params)
{
    (void)params;

    printf("Started command task\n");

    microrl_init(&rl, command_print);

    microrl_set_execute_callback(&rl, command_exec);

    for (;;) {
        int c = getchar();

        if (c >= 0)
            microrl_insert_char(&rl, c);

        taskYIELD();
    }

    vTaskDelete(NULL);
}

void command_init(void)
{
    xTaskCreate(&command_task, "command_task", TASK_STACK_SIZE, NULL, TASK_PRIORITY, &xCommandTask);
}