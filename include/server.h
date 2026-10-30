#ifndef SERVER_H
#define SERVER_H

#include "common.h"

typedef struct {
    int port;
    int server_socket;
    int client_sockets[MAX_CLIENTS];
    int max_clients;
    volatile int running;
    char data_dir[256];
    char log_file[256];
} ServerConfig;

int initServer(int port, const char *data_dir, const char *log_file);
int runServer(void);
void stopServer(void);
int sendToClient(int client_socket, const char *message);

#endif /* SERVER_H */
