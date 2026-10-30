#include "room.h"
#include "user.h"
#include "logger.h"

static ChatRoom g_rooms[MAX_ROOMS];
static int g_room_count = 0;
static int g_next_room_id = 1;

int initRoomManager(void) {
    memset(g_rooms, 0, sizeof(g_rooms));
    g_room_count = 0;
    g_next_room_id = 1;

    /* Pre-populate default chat rooms from Section 5.6 */
    char err[128];
    createRoom("general", err, sizeof(err));
    createRoom("coding", err, sizeof(err));
    createRoom("gaming", err, sizeof(err));
    createRoom("random", err, sizeof(err));

    return 0;
}

void cleanupRoomManager(void) {
    memset(g_rooms, 0, sizeof(g_rooms));
    g_room_count = 0;
}

int getRoomCount(void) {
    return g_room_count;
}

ChatRoom* getRoomByIndex(int index) {
    if (index >= 0 && index < g_room_count) {
        return &g_rooms[index];
    }
    return NULL;
}

ChatRoom* findRoomById(int room_id) {
    for (int i = 0; i < g_room_count; i++) {
        if (g_rooms[i].room_id == room_id) {
            return &g_rooms[i];
        }
    }
    return NULL;
}

ChatRoom* findRoomByName(const char *name) {
    if (!name) return NULL;
    /* Strip leading '#' or '/' if user passed it like #coding or /coding */
    const char *p = name;
    if (*p == '#' || *p == '/') {
        p++;
    }
    for (int i = 0; i < g_room_count; i++) {
        if (strcasecmp(g_rooms[i].name, p) == 0) {
            return &g_rooms[i];
        }
    }
    return NULL;
}

void updateRoomUserCount(int room_id) {
    ChatRoom *room = findRoomById(room_id);
    if (!room) return;

    int count = 0;
    int total_users = getUserCount();
    for (int i = 0; i < total_users; i++) {
        User *u = getUserByIndex(i);
        if (u && u->online && u->room_id == room_id) {
            count++;
        }
    }
    room->user_count = count;
}

/* Section 9: createRoom() */
ChatRoom* createRoom(const char *name, char *err_msg, size_t err_len) {
    if (!name || strlen(name) == 0) {
        if (err_msg) snprintf(err_msg, err_len, "Room name cannot be empty");
        return NULL;
    }

    const char *clean_name = name;
    if (*clean_name == '#' || *clean_name == '/') {
        clean_name++;
    }

    if (strlen(clean_name) >= MAX_ROOM_NAME_LEN) {
        if (err_msg) snprintf(err_msg, err_len, "Room name exceeds max length of %d", MAX_ROOM_NAME_LEN - 1);
        return NULL;
    }

    if (findRoomByName(clean_name) != NULL) {
        if (err_msg) snprintf(err_msg, err_len, "Room '%s' already exists", clean_name);
        return NULL;
    }

    if (g_room_count >= MAX_ROOMS) {
        if (err_msg) snprintf(err_msg, err_len, "Maximum number of rooms reached (%d)", MAX_ROOMS);
        return NULL;
    }

    ChatRoom *room = &g_rooms[g_room_count++];
    room->room_id = g_next_room_id++;
    strncpy(room->name, clean_name, MAX_ROOM_NAME_LEN - 1);
    room->name[MAX_ROOM_NAME_LEN - 1] = '\0';
    room->user_count = 0;

    writeLog(LOG_LEVEL_INFO, "Created room: '%s' (ID: %d)", room->name, room->room_id);
    return room;
}

/* Section 9: joinRoom() */
int joinRoom(int user_id, const char *room_name, char *err_msg, size_t err_len) {
    User *user = findUserById(user_id);
    if (!user) {
        if (err_msg) snprintf(err_msg, err_len, "User not found");
        return -1;
    }

    ChatRoom *target_room = findRoomByName(room_name);
    if (!target_room) {
        /* Auto-create if not found or report error */
        target_room = createRoom(room_name, err_msg, err_len);
        if (!target_room) {
            return -1;
        }
    }

    int old_room_id = user->room_id;
    user->room_id = target_room->room_id;

    updateRoomUserCount(old_room_id);
    updateRoomUserCount(target_room->room_id);

    writeLog(LOG_LEVEL_INFO, "User '%s' joined room '%s'", user->username, target_room->name);
    return target_room->room_id;
}

/* Section 9: leaveRoom() */
int leaveRoom(int user_id, char *err_msg, size_t err_len) {
    User *user = findUserById(user_id);
    if (!user) {
        if (err_msg) snprintf(err_msg, err_len, "User not found");
        return -1;
    }

    ChatRoom *default_room = findRoomByName("general");
    if (!default_room) {
        default_room = createRoom("general", NULL, 0);
    }

    int old_room_id = user->room_id;
    if (default_room) {
        user->room_id = default_room->room_id;
    }

    updateRoomUserCount(old_room_id);
    if (default_room) {
        updateRoomUserCount(default_room->room_id);
    }

    writeLog(LOG_LEVEL_INFO, "User '%s' left room back to general", user->username);
    return 0;
}

void listRooms(char *buffer, size_t size) {
    size_t offset = 0;
    offset += snprintf(buffer + offset, size - offset,
        "================ AVAILABLE ROOMS (%d) ================\n"
        " %-5s %-20s %-15s\n"
        "----------------------------------------------------\n",
        g_room_count, "ID", "ROOM NAME", "ACTIVE USERS");

    for (int i = 0; i < g_room_count; i++) {
        updateRoomUserCount(g_rooms[i].room_id);
        int written = snprintf(buffer + offset, size - offset,
            " %-5d #%-19s %-15d\n",
            g_rooms[i].room_id,
            g_rooms[i].name,
            g_rooms[i].user_count);
        if (written > 0 && (size_t)written < size - offset) {
            offset += written;
        } else {
            break;
        }
    }
    snprintf(buffer + offset, size - offset, "====================================================\n");
}
