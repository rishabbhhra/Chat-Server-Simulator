#include "storage.h"
#include "user.h"
#include "room.h"
#include "message.h"
#include "logger.h"
#include <sys/stat.h>

static char g_data_dir[256] = "data";

int initStorage(const char *data_dir) {
    if (data_dir && strlen(data_dir) > 0) {
        strncpy(g_data_dir, data_dir, sizeof(g_data_dir) - 1);
        g_data_dir[sizeof(g_data_dir) - 1] = '\0';
    }

    /* Create directory if it doesn't exist */
    struct stat st;
    if (stat(g_data_dir, &st) == -1) {
        mkdir(g_data_dir, 0755);
    }
    return 0;
}

/* Section 9: saveData() */
int saveData(void) {
    char filepath[512];

    /* 1. Save Users */
    snprintf(filepath, sizeof(filepath), "%s/users.txt", g_data_dir);
    FILE *fp = fopen(filepath, "w");
    if (!fp) {
        writeLog(LOG_LEVEL_ERROR, "Failed to open users file for saving: %s", filepath);
        return -1;
    }

    int user_count = getUserCount();
    fprintf(fp, "# User Data File: ID, Username, Password, Flags\n");
    for (int i = 0; i < user_count; i++) {
        User *u = getUserByIndex(i);
        if (u) {
            fprintf(fp, "%d %s %s %u\n", u->id, u->username, u->password, u->flags & ~USER_FLAG_ONLINE);
        }
    }
    fclose(fp);

    /* 2. Save Rooms */
    snprintf(filepath, sizeof(filepath), "%s/rooms.txt", g_data_dir);
    fp = fopen(filepath, "w");
    if (!fp) {
        writeLog(LOG_LEVEL_ERROR, "Failed to open rooms file for saving: %s", filepath);
        return -1;
    }

    int room_count = getRoomCount();
    fprintf(fp, "# Room Data File: ID, Name\n");
    for (int i = 0; i < room_count; i++) {
        ChatRoom *r = getRoomByIndex(i);
        if (r) {
            fprintf(fp, "%d %s\n", r->room_id, r->name);
        }
    }
    fclose(fp);

    /* 3. Save Message History */
    snprintf(filepath, sizeof(filepath), "%s/history.txt", g_data_dir);
    fp = fopen(filepath, "w");
    if (!fp) {
        writeLog(LOG_LEVEL_ERROR, "Failed to open history file for saving: %s", filepath);
        return -1;
    }

    int msg_count = getMessageCount();
    fprintf(fp, "# Message History: SenderID, ReceiverID, Flags, Timestamp, Content\n");
    for (int i = 0; i < msg_count; i++) {
        Message *m = getMessageByIndex(i);
        if (m) {
            fprintf(fp, "%d|%d|%u|%s|%s\n",
                    m->sender_id, m->receiver_id, m->flags, m->timestamp, m->message);
        }
    }
    fclose(fp);

    writeLog(LOG_LEVEL_INFO, "System data successfully persisted to directory '%s'", g_data_dir);
    return 0;
}

/* Section 9: loadData() */
int loadData(void) {
    char filepath[512];
    char line[1024];

    /* 1. Load Rooms */
    snprintf(filepath, sizeof(filepath), "%s/rooms.txt", g_data_dir);
    FILE *fp = fopen(filepath, "r");
    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            trim_newline(line);
            if (line[0] == '#' || strlen(line) == 0) continue;
            int room_id;
            char room_name[MAX_ROOM_NAME_LEN];
            if (sscanf(line, "%d %49s", &room_id, room_name) == 2) {
                if (findRoomByName(room_name) == NULL) {
                    createRoom(room_name, NULL, 0);
                }
            }
        }
        fclose(fp);
    }

    /* 2. Load Users */
    snprintf(filepath, sizeof(filepath), "%s/users.txt", g_data_dir);
    fp = fopen(filepath, "r");
    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            trim_newline(line);
            if (line[0] == '#' || strlen(line) == 0) continue;
            int id;
            char username[MAX_USERNAME_LEN];
            char password[MAX_PASSWORD_LEN];
            unsigned int flags = 0;

            if (sscanf(line, "%d %49s %49s %u", &id, username, password, &flags) >= 3) {
                if (findUserByUsername(username) == NULL) {
                    registerUser(username, password, NULL, 0);
                    User *u = findUserByUsername(username);
                    if (u) {
                        u->flags = flags & ~USER_FLAG_ONLINE; // Reset online flag on load
                    }
                }
            }
        }
        fclose(fp);
    }

    /* 3. Load History */
    snprintf(filepath, sizeof(filepath), "%s/history.txt", g_data_dir);
    fp = fopen(filepath, "r");
    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            trim_newline(line);
            if (line[0] == '#' || strlen(line) == 0) continue;

            char *token = strtok(line, "|");
            if (!token) continue;
            int sender_id = atoi(token);

            token = strtok(NULL, "|");
            if (!token) continue;
            int receiver_id = atoi(token);

            token = strtok(NULL, "|");
            if (!token) continue;
            unsigned int flags = (unsigned int)atoi(token);

            token = strtok(NULL, "|");
            if (!token) continue;
            char timestamp[MAX_TIMESTAMP_LEN];
            strncpy(timestamp, token, sizeof(timestamp) - 1);
            timestamp[sizeof(timestamp) - 1] = '\0';

            token = strtok(NULL, "");
            if (!token) continue;

            recordMessage(sender_id, receiver_id, token, flags);
        }
        fclose(fp);
    }

    writeLog(LOG_LEVEL_INFO, "System data loaded: %d users, %d rooms, %d messages",
             getUserCount(), getRoomCount(), getMessageCount());
    return 0;
}
