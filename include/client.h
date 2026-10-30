#ifndef CLIENT_H
#define CLIENT_H

#include "common.h"

typedef struct {
    char server_ip[64];
    int server_port;
    int socket_fd;
    volatile int running;
    char username[MAX_USERNAME_LEN];
    char current_room[MAX_ROOM_NAME_LEN];
    int logged_in;
} ClientConfig;

int initClient(const char *server_ip, int port);
int runClient(void);
void renderClientBanner(const char *username, const char *status, const char *room);

#endif /* CLIENT_H */
