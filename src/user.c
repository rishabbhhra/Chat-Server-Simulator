#include "user.h"
#include "room.h"
#include "logger.h"
#include "stats.h"
#include "server.h"

/* Dynamic array of User pointers (demonstrates pointers and dynamic memory allocation) */
static User **g_users = NULL;
static int g_user_count = 0;
static int g_user_capacity = 0;
static int g_next_user_id = 1;

int initUserManager(void) {
    g_user_capacity = 16;
    g_users = (User **)malloc(sizeof(User *) * g_user_capacity);
    if (!g_users) {
        writeLog(LOG_LEVEL_ERROR, "Failed to allocate memory for User Manager");
        return -1;
    }
    g_user_count = 0;
    g_next_user_id = 1;

    /* Pre-register an admin user for management */
    char err[128];
    registerUser("admin", "admin123", err, sizeof(err));
    User *admin = findUserByUsername("admin");
    if (admin) {
        admin->flags |= USER_FLAG_ADMIN;
    }
    return 0;
}

void cleanupUserManager(void) {
    if (g_users) {
        for (int i = 0; i < g_user_count; i++) {
            if (g_users[i]) {
                free(g_users[i]);
            }
        }
        free(g_users);
        g_users = NULL;
    }
    g_user_count = 0;
    g_user_capacity = 0;
}

int getUserCount(void) {
    return g_user_count;
}

int getOnlineUserCount(void) {
    int count = 0;
    for (int i = 0; i < g_user_count; i++) {
        if (g_users[i]->online && (g_users[i]->flags & USER_FLAG_ONLINE)) {
            count++;
        }
    }
    return count;
}

User* getUserByIndex(int index) {
    if (index >= 0 && index < g_user_count) {
        return g_users[index];
    }
    return NULL;
}

/* Linear search by username */
User* findUserByUsername(const char *username) {
    if (!username) return NULL;
    for (int i = 0; i < g_user_count; i++) {
        if (strcasecmp(g_users[i]->username, username) == 0) {
            return g_users[i];
        }
    }
    return NULL;
}

/* Search by user id */
User* findUserById(int id) {
    for (int i = 0; i < g_user_count; i++) {
        if (g_users[i]->id == id) {
            return g_users[i];
        }
    }
    return NULL;
}

/* Search by connected socket file descriptor */
User* findUserBySocket(int socket_fd) {
    if (socket_fd < 0) return NULL;
    for (int i = 0; i < g_user_count; i++) {
        if (g_users[i]->online && g_users[i]->socket_fd == socket_fd) {
            return g_users[i];
        }
    }
    return NULL;
}

/* Section 9: registerUser() */
int registerUser(const char *username, const char *password, char *err_msg, size_t err_len) {
    if (!username || strlen(username) == 0) {
        if (err_msg) snprintf(err_msg, err_len, "Username cannot be empty");
        return -1;
    }
    if (strlen(username) >= MAX_USERNAME_LEN) {
        if (err_msg) snprintf(err_msg, err_len, "Username exceeds maximum length of %d", MAX_USERNAME_LEN - 1);
        return -1;
    }
    if (!password || strlen(password) == 0) {
        if (err_msg) snprintf(err_msg, err_len, "Password cannot be empty");
        return -1;
    }
    if (strlen(password) >= MAX_PASSWORD_LEN) {
        if (err_msg) snprintf(err_msg, err_len, "Password exceeds maximum length of %d", MAX_PASSWORD_LEN - 1);
        return -1;
    }

    /* Check for duplicate username */
    if (findUserByUsername(username) != NULL) {
        if (err_msg) snprintf(err_msg, err_len, "Username '%s' already exists", username);
        writeLog(LOG_LEVEL_WARN, "Registration failed: duplicate username '%s'", username);
        return -1;
    }

    /* Expand capacity dynamically if needed */
    if (g_user_count >= g_user_capacity) {
        int new_capacity = g_user_capacity * 2;
        User **new_array = (User **)realloc(g_users, sizeof(User *) * new_capacity);
        if (!new_array) {
            if (err_msg) snprintf(err_msg, err_len, "Server internal error: memory allocation failed");
            return -1;
        }
        g_users = new_array;
        g_user_capacity = new_capacity;
    }

    /* Allocate and initialize new User structure */
    User *new_user = (User *)malloc(sizeof(User));
    if (!new_user) {
        if (err_msg) snprintf(err_msg, err_len, "Memory allocation failed for new user");
        return -1;
    }

    new_user->id = g_next_user_id++;
    strncpy(new_user->username, username, MAX_USERNAME_LEN - 1);
    new_user->username[MAX_USERNAME_LEN - 1] = '\0';
    strncpy(new_user->password, password, MAX_PASSWORD_LEN - 1);
    new_user->password[MAX_PASSWORD_LEN - 1] = '\0';
    new_user->socket_fd = -1;
    new_user->room_id = 1; /* Default room id: general */
    new_user->online = 0;
    new_user->flags = USER_FLAG_ACTIVE;

    g_users[g_user_count++] = new_user;

    recordRegistration();
    writeLog(LOG_LEVEL_INFO, "User registered: '%s' (ID: %d)", username, new_user->id);
    return new_user->id;
}

/* Section 9: loginUser() */
User* loginUser(int socket_fd, const char *username, const char *password, char *err_msg, size_t err_len) {
    if (!username || !password) {
        if (err_msg) snprintf(err_msg, err_len, "Invalid username or password");
        return NULL;
    }

    User *user = findUserByUsername(username);
    if (!user) {
        if (err_msg) snprintf(err_msg, err_len, "User '%s' not found", username);
        writeLog(LOG_LEVEL_WARN, "Login failed: non-existent user '%s'", username);
        return NULL;
    }

    if (strcmp(user->password, password) != 0) {
        if (err_msg) snprintf(err_msg, err_len, "Incorrect password");
        writeLog(LOG_LEVEL_WARN, "Login failed: incorrect password for '%s'", username);
        return NULL;
    }

    if (user->online && (user->flags & USER_FLAG_ONLINE)) {
        if (user->socket_fd == socket_fd) {
            /* Already logged in on this socket */
            return user;
        }
        /* Transfer session to new connection */
        writeLog(LOG_LEVEL_INFO, "User '%s' re-authenticating; transferring from socket %d to %d",
                 username, user->socket_fd, socket_fd);
        if (user->socket_fd > 0) {
            sendToClient(user->socket_fd, "[SESSION] Logged in from another window or tab.\n");
        }
    }

    /* Update user online status and socket descriptor */
    user->socket_fd = socket_fd;
    user->online = 1;
    user->flags |= USER_FLAG_ONLINE;

    /* Update room count for the user's room */
    updateRoomUserCount(user->room_id);

    recordLogin();
    writeLog(LOG_LEVEL_INFO, "User '%s' logged in successfully on socket %d", username, socket_fd);
    return user;
}

/* Section 9: logoutUser() */
int logoutUser(int user_id) {
    User *user = findUserById(user_id);
    if (!user) return -1;

    user->online = 0;
    user->flags &= ~USER_FLAG_ONLINE;
    user->socket_fd = -1;

    updateRoomUserCount(user->room_id);
    writeLog(LOG_LEVEL_INFO, "User '%s' logged out", user->username);
    return 0;
}

/* Section 9: disconnectUser() */
int disconnectUser(int socket_fd) {
    User *user = findUserBySocket(socket_fd);
    if (user) {
        writeLog(LOG_LEVEL_INFO, "Client disconnected: '%s' (socket %d)", user->username, socket_fd);
        user->online = 0;
        user->flags &= ~USER_FLAG_ONLINE;
        user->socket_fd = -1;
        updateRoomUserCount(user->room_id);
        recordDisconnection();
        return user->id;
    }
    return -1;
}

int removeUser(int id) {
    for (int i = 0; i < g_user_count; i++) {
        if (g_users[i]->id == id) {
            writeLog(LOG_LEVEL_INFO, "User removed: '%s'", g_users[i]->username);
            free(g_users[i]);
            for (int j = i; j < g_user_count - 1; j++) {
                g_users[j] = g_users[j + 1];
            }
            g_user_count--;
            return 0;
        }
    }
    return -1;
}

/* Sort users alphabetically using qsort with comparator function */
static int compareUsers(const void *a, const void *b) {
    User *u1 = *(User **)a;
    User *u2 = *(User **)b;
    return strcasecmp(u1->username, u2->username);
}

void sortUsersAlphabetically(User **sorted_array, int *count) {
    *count = 0;
    for (int i = 0; i < g_user_count; i++) {
        if (g_users[i]->online && (g_users[i]->flags & USER_FLAG_ONLINE)) {
            sorted_array[(*count)++] = g_users[i];
        }
    }
    if (*count > 1) {
        qsort(sorted_array, *count, sizeof(User *), compareUsers);
    }
}

/* Section 9: listUsers() */
void listUsers(char *buffer, size_t size) {
    User *online_users[MAX_USERS];
    int count = 0;
    sortUsersAlphabetically(online_users, &count);

    if (count == 0) {
        snprintf(buffer, size, "No users currently online.\n");
        return;
    }

    size_t offset = 0;
    offset += snprintf(buffer + offset, size - offset,
        "================ ONLINE USERS (%d) ================\n"
        " %-15s %-15s %-10s\n"
        "---------------------------------------------------\n",
        count, "USERNAME", "ROOM", "ROLE");

    for (int i = 0; i < count; i++) {
        ChatRoom *r = findRoomById(online_users[i]->room_id);
        const char *room_name = r ? r->name : "Lobby";
        const char *role = (online_users[i]->flags & USER_FLAG_ADMIN) ? "Admin" : "Member";

        int written = snprintf(buffer + offset, size - offset,
            " %-15s %-15s %-10s\n",
            online_users[i]->username,
            room_name,
            role);
        if (written > 0 && (size_t)written < size - offset) {
            offset += written;
        } else {
            break;
        }
    }
    snprintf(buffer + offset, size - offset, "===================================================\n");
}
