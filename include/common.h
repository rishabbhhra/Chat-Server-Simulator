#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define DEFAULT_PORT 8080
#define DEFAULT_IP "127.0.0.1"
#define MAX_CLIENTS 64
#define BUFFER_SIZE 2048
#define MAX_USERNAME_LEN 50
#define MAX_PASSWORD_LEN 50
#define MAX_ROOM_NAME_LEN 50
#define MAX_MESSAGE_LEN 500
#define MAX_TIMESTAMP_LEN 30
#define MAX_ROOMS 32
#define MAX_USERS 256
#define MAX_HISTORY 1000

#define DEFAULT_DATA_DIR "data"
#define DEFAULT_LOG_DIR "logs"

/* Bitwise user permission/status flags */
#define USER_FLAG_NONE       0x00
#define USER_FLAG_ACTIVE     (1 << 0) // 0x01: Account is active
#define USER_FLAG_ONLINE     (1 << 1) // 0x02: Connected and logged in
#define USER_FLAG_ADMIN      (1 << 2) // 0x04: Administrator role
#define USER_FLAG_MUTED      (1 << 3) // 0x08: Muted user

/* Bitwise message attribute flags */
#define MSG_FLAG_NONE        0x00
#define MSG_FLAG_PUBLIC      (1 << 0) // 0x01: Public room message
#define MSG_FLAG_PRIVATE     (1 << 1) // 0x02: Direct private message
#define MSG_FLAG_SYSTEM      (1 << 2) // 0x04: Server notification / system info
#define MSG_FLAG_ALERT       (1 << 3) // 0x08: High priority / alert

/* Section 8.1: User Structure */
typedef struct {
    int id;
    char username[MAX_USERNAME_LEN];
    char password[MAX_PASSWORD_LEN];
    int socket_fd;
    int room_id;
    int online;
    unsigned int flags; /* Bitwise status flags */
} User;

/* Section 8.2: Chat Room Structure */
typedef struct {
    int room_id;
    char name[MAX_ROOM_NAME_LEN];
    int user_count;
} ChatRoom;

/* Section 8.3: Message Structure */
typedef struct {
    int sender_id;
    int receiver_id; /* -1 for broadcast to room, or receiver user ID for private */
    char message[MAX_MESSAGE_LEN];
    char timestamp[MAX_TIMESTAMP_LEN];
    unsigned int flags; /* Bitwise message flags */
} Message;

/* Utility helper to get formatted current time */
static inline void get_current_timestamp(char *buffer, size_t size) {
    time_t rawtime;
    struct tm *info;
    time(&rawtime);
    info = localtime(&rawtime);
    if (info != NULL) {
        strftime(buffer, size, "%Y-%m-%d %H:%M:%S", info);
    } else {
        snprintf(buffer, size, "UNKNOWN_TIME");
    }
}

static inline void get_short_time(char *buffer, size_t size) {
    time_t rawtime;
    struct tm *info;
    time(&rawtime);
    info = localtime(&rawtime);
    if (info != NULL) {
        strftime(buffer, size, "%H:%M", info);
    } else {
        snprintf(buffer, size, "??:??");
    }
}

/* Trim trailing newline and carriage returns */
static inline void trim_newline(char *str) {
    if (!str) return;
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r' || str[len - 1] == ' ')) {
        str[len - 1] = '\0';
        len--;
    }
}

#endif /* COMMON_H */
