#ifndef COMMAND_H
#define COMMAND_H

#include "common.h"

/* Function pointer type for command handlers (Advanced Concept: Function Pointers) */
typedef int (*CommandHandler)(int client_socket, User *user, const char *args);

typedef struct {
    const char *command_name;
    CommandHandler handler;
    const char *description;
    const char *usage;
    int requires_auth;
} CommandEntry;

void initCommandSystem(void);

/* Section 9 Required Function */
int handleCommand(int client_socket, const char *raw_command);

const CommandEntry* getCommandList(int *count);
void getHelpText(char *buffer, size_t size, int is_admin);

#endif /* COMMAND_H */
