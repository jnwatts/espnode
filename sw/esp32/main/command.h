#ifndef COMMAND_H
#define COMMAND_H

typedef struct {
    int (*func)(int argc, const char * const * argv);
    const char *argv0;
} command_t;

int command_help(int argc, const char * const * argv);

void command_init(void);

#endif // COMMAND_H