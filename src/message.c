#include "message.h"
#include "user.h"
#include "room.h"
#include "server.h"
#include "logger.h"
#include "stats.h"

static Message *g_history = NULL;
static int g_history_count = 0;
static int g_history_capacity = 0;

int initMessageManager(void) {
    g_history_capacity = 64;
    g_history = (Message *)malloc(sizeof(Message) * g_history_capacity);
    if (!g_history) {
        writeLog(LOG_LEVEL_ERROR, "Failed to allocate memory for Message History");
        return -1;
    }
    g_history_count = 0;
    return 0;
}

void cleanupMessageManager(void) {
    if (g_history) {
        free(g_history);
        g_history = NULL;
    }
    g_history_count = 0;
    g_history_capacity = 0;
}

int getMessageCount(void) {
    return g_history_count;
}

Message* getMessageByIndex(int index) {
    if (index >= 0 && index < g_history_count) {
        return &g_history[index];
    }
    return NULL;
}

int recordMessage(int sender_id, int receiver_id, const char *text, unsigned int flags) {
    if (!text) return -1;

    if (g_history_count >= g_history_capacity) {
        if (g_history_capacity >= MAX_HISTORY) {
            /* Shift old messages left to discard oldest */
            memmove(&g_history[0], &g_history[1], sizeof(Message) * (g_history_capacity - 1));
            g_history_count = g_history_capacity - 1;
        } else {
            int new_cap = g_history_capacity * 2;
            if (new_cap > MAX_HISTORY) new_cap = MAX_HISTORY;
            Message *new_arr = (Message *)realloc(g_history, sizeof(Message) * new_cap);
            if (!new_arr) return -1;
            g_history = new_arr;
            g_history_capacity = new_cap;
        }
    }

    Message *m = &g_history[g_history_count++];
    m->sender_id = sender_id;
    m->receiver_id = receiver_id;
    strncpy(m->message, text, MAX_MESSAGE_LEN - 1);
    m->message[MAX_MESSAGE_LEN - 1] = '\0';
    get_current_timestamp(m->timestamp, sizeof(m->timestamp));
    m->flags = flags;

    return g_history_count - 1;
}

/* Section 9: broadcastMessage() */
void broadcastMessage(int room_id, const char *text, int exclude_socket_fd) {
    int total_users = getUserCount();
    for (int i = 0; i < total_users; i++) {
        User *u = getUserByIndex(i);
        if (u && u->online && (u->flags & USER_FLAG_ONLINE)) {
            if (u->room_id == room_id && u->socket_fd != exclude_socket_fd && u->socket_fd > 0) {
                sendToClient(u->socket_fd, text);
            }
        }
    }
}

/* Section 9: sendMessage() - Public Message to current room */
int sendMessage(int sender_id, const char *text, char *err_msg, size_t err_len) {
    if (!text || strlen(text) == 0) {
        if (err_msg) snprintf(err_msg, err_len, "Message cannot be empty");
        return -1;
    }

    if (strlen(text) >= MAX_MESSAGE_LEN) {
        if (err_msg) snprintf(err_msg, err_len, "Message exceeds max length of %d chars", MAX_MESSAGE_LEN - 1);
        return -1;
    }

    User *sender = findUserById(sender_id);
    if (!sender || !sender->online) {
        if (err_msg) snprintf(err_msg, err_len, "Sender not authenticated or offline");
        return -1;
    }

    if (sender->flags & USER_FLAG_MUTED) {
        if (err_msg) snprintf(err_msg, err_len, "You are muted and cannot send messages");
        return -1;
    }

    char time_str[10];
    get_short_time(time_str, sizeof(time_str));

    char formatted_msg[BUFFER_SIZE];
    snprintf(formatted_msg, sizeof(formatted_msg), "[%s] %s: %s\n", time_str, sender->username, text);

    /* Record in history */
    recordMessage(sender_id, -1, text, MSG_FLAG_PUBLIC);
    recordPublicMessage();

    /* Broadcast to all users in the same room */
    broadcastMessage(sender->room_id, formatted_msg, sender->socket_fd);

    ChatRoom *room = findRoomById(sender->room_id);
    writeLog(LOG_LEVEL_INFO, "Public msg in #%s by '%s': %s",
             room ? room->name : "unknown", sender->username, text);

    return 0;
}

/* Section 9: sendPrivateMessage() - Directed to a specific user */
int sendPrivateMessage(int sender_id, const char *receiver_username, const char *text, char *err_msg, size_t err_len) {
    if (!receiver_username || strlen(receiver_username) == 0) {
        if (err_msg) snprintf(err_msg, err_len, "Recipient username cannot be empty");
        return -1;
    }

    if (!text || strlen(text) == 0) {
        if (err_msg) snprintf(err_msg, err_len, "Message content cannot be empty");
        return -1;
    }

    if (strlen(text) >= MAX_MESSAGE_LEN) {
        if (err_msg) snprintf(err_msg, err_len, "Message exceeds max length of %d chars", MAX_MESSAGE_LEN - 1);
        return -1;
    }

    User *sender = findUserById(sender_id);
    if (!sender || !sender->online) {
        if (err_msg) snprintf(err_msg, err_len, "Sender not authenticated or offline");
        return -1;
    }

    User *receiver = findUserByUsername(receiver_username);
    if (!receiver) {
        if (err_msg) snprintf(err_msg, err_len, "User '%s' does not exist", receiver_username);
        return -1;
    }

    if (!receiver->online || receiver->socket_fd <= 0) {
        if (err_msg) snprintf(err_msg, err_len, "User '%s' is currently offline", receiver_username);
        return -1;
    }

    char time_str[10];
    get_short_time(time_str, sizeof(time_str));

    char formatted_for_receiver[BUFFER_SIZE];
    snprintf(formatted_for_receiver, sizeof(formatted_for_receiver),
             "[%s] [PM from %s]: %s\n", time_str, sender->username, text);

    sendToClient(receiver->socket_fd, formatted_for_receiver);

    /* Also acknowledge to sender */
    char ack_for_sender[BUFFER_SIZE];
    snprintf(ack_for_sender, sizeof(ack_for_sender),
             "[%s] [PM to %s]: %s\n", time_str, receiver->username, text);
    sendToClient(sender->socket_fd, ack_for_sender);

    /* Record in history */
    recordMessage(sender_id, receiver->id, text, MSG_FLAG_PRIVATE);
    recordPrivateMessage();

    writeLog(LOG_LEVEL_INFO, "Private msg from '%s' to '%s': %s", sender->username, receiver->username, text);
    return 0;
}

/* Section 9: showHistory() */
void showHistory(int room_id, char *buffer, size_t size, int limit) {
    if (limit <= 0) limit = 20;

    ChatRoom *room = findRoomById(room_id);
    size_t offset = 0;
    offset += snprintf(buffer + offset, size - offset,
        "================ MESSAGE HISTORY (#%s) ================\n",
        room ? room->name : "all");

    int shown = 0;
    /* Walk backwards to find latest matching messages */
    int start_index = g_history_count - 1;
    int matches[100];
    int match_count = 0;

    for (int i = start_index; i >= 0 && match_count < limit && match_count < 100; i--) {
        Message *m = &g_history[i];
        if (room_id == -1 || (m->flags & MSG_FLAG_PUBLIC)) {
            User *sender = findUserById(m->sender_id);
            if (room_id == -1 || (sender && sender->room_id == room_id)) {
                matches[match_count++] = i;
            }
        }
    }

    /* Print in chronological order */
    for (int i = match_count - 1; i >= 0; i--) {
        Message *m = &g_history[matches[i]];
        User *sender = findUserById(m->sender_id);
        const char *s_name = sender ? sender->username : "unknown";

        int written = snprintf(buffer + offset, size - offset,
            "[%s] %s: %s\n",
            m->timestamp, s_name, m->message);
        if (written > 0 && (size_t)written < size - offset) {
            offset += written;
            shown++;
        } else {
            break;
        }
    }

    if (shown == 0) {
        offset += snprintf(buffer + offset, size - offset, " (No message history available for this room)\n");
    }

    snprintf(buffer + offset, size - offset, "========================================================\n");
}
