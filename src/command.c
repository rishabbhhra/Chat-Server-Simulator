#include "command.h"
#include "user.h"
#include "room.h"
#include "message.h"
#include "stats.h"
#include "storage.h"
#include "logger.h"
#include "server.h"

/* Forward declarations for command handler functions (Function Pointers) */
static int cmd_register(int sock, User *user, const char *args);
static int cmd_login(int sock, User *user, const char *args);
static int cmd_logout(int sock, User *user, const char *args);
static int cmd_users(int sock, User *user, const char *args);
static int cmd_join(int sock, User *user, const char *args);
static int cmd_leave(int sock, User *user, const char *args);
static int cmd_rooms(int sock, User *user, const char *args);
static int cmd_msg(int sock, User *user, const char *args);
static int cmd_history(int sock, User *user, const char *args);
static int cmd_stats(int sock, User *user, const char *args);
static int cmd_help(int sock, User *user, const char *args);
static int cmd_quit(int sock, User *user, const char *args);
static int cmd_kick(int sock, User *user, const char *args);
static int cmd_ping(int sock, User *user, const char *args);
static int cmd_broadcast(int sock, User *user, const char *args);

/* Command Table (Advanced Concept: Function Pointers) */
static CommandEntry g_command_table[] = {
    {"/register",  cmd_register,  "Register a new account",        "/register <username> <password>", 0},
    {"/login",     cmd_login,     "Log into existing account",     "/login <username> <password>",    0},
    {"/logout",    cmd_logout,    "Log out of current account",    "/logout",                         1},
    {"/users",     cmd_users,     "List online users",             "/users",                          1},
    {"/join",      cmd_join,      "Join or create a chat room",    "/join <room>",                    1},
    {"/leave",     cmd_leave,     "Leave room back to general",    "/leave",                          1},
    {"/rooms",     cmd_rooms,     "List available chat rooms",     "/rooms",                          1},
    {"/msg",       cmd_msg,       "Send a private message",        "/msg <user> <message>",           1},
    {"/history",   cmd_history,   "View recent chat history",      "/history",                        1},
    {"/stats",     cmd_stats,     "View server statistics",        "/stats",                          0},
    {"/ping",      cmd_ping,      "Ping server to test latency",   "/ping",                           0},
    {"/broadcast", cmd_broadcast, "Admin: Broadcast to all rooms", "/broadcast <message>",           1},
    {"/help",      cmd_help,      "Show list of commands",         "/help",                           0},
    {"/quit",      cmd_quit,      "Disconnect from the server",    "/quit",                           0},
    {"/kick",      cmd_kick,      "Admin: Kick user from server",  "/kick <username>",                1}
};

static int g_num_commands = sizeof(g_command_table) / sizeof(g_command_table[0]);

void initCommandSystem(void) {
    /* Ready for command dispatching */
}

const CommandEntry* getCommandList(int *count) {
    if (count) *count = g_num_commands;
    return g_command_table;
}

void getHelpText(char *buffer, size_t size, int is_admin) {
    size_t offset = 0;
    offset += snprintf(buffer + offset, size - offset,
        "=================== AVAILABLE COMMANDS ===================\n");

    for (int i = 0; i < g_num_commands; i++) {
        if (strcmp(g_command_table[i].command_name, "/kick") == 0 && !is_admin) {
            continue; /* Hide admin command for regular users */
        }
        int written = snprintf(buffer + offset, size - offset,
            " %-25s - %s\n",
            g_command_table[i].usage,
            g_command_table[i].description);
        if (written > 0 && (size_t)written < size - offset) {
            offset += written;
        } else {
            break;
        }
    }
    snprintf(buffer + offset, size - offset,
        " (Tip: Plain text without '/' sends public message to room)\n"
        "==========================================================\n");
}

/* Section 9: handleCommand() */
int handleCommand(int client_socket, const char *raw_command) {
    if (!raw_command) return -1;

    char cmd_copy[BUFFER_SIZE];
    strncpy(cmd_copy, raw_command, sizeof(cmd_copy) - 1);
    cmd_copy[sizeof(cmd_copy) - 1] = '\0';
    trim_newline(cmd_copy);

    if (strlen(cmd_copy) == 0) {
        return 0;
    }

    User *user = findUserBySocket(client_socket);

    /* If input does NOT start with '/', treat as public room message */
    if (cmd_copy[0] != '/') {
        if (!user || !user->online) {
            sendToClient(client_socket, "[ERROR] Please /login or /register before chatting.\n");
            return -1;
        }
        char err[128];
        if (sendMessage(user->id, cmd_copy, err, sizeof(err)) != 0) {
            char resp[256];
            snprintf(resp, sizeof(resp), "[ERROR] %s\n", err);
            sendToClient(client_socket, resp);
        } else {
            saveData();
        }
        return 0;
    }

    recordCommand();

    /* Split command name and arguments */
    char *cmd_name = strtok(cmd_copy, " ");
    char *args = strtok(NULL, "");
    if (args) {
        while (*args == ' ') args++;
    }

    /* Search command table */
    for (int i = 0; i < g_num_commands; i++) {
        if (strcasecmp(g_command_table[i].command_name, cmd_name) == 0) {
            if (g_command_table[i].requires_auth && (!user || !user->online)) {
                sendToClient(client_socket, "[ERROR] Authentication required. Please /login first.\n");
                return -1;
            }
            /* Invoke via function pointer */
            return g_command_table[i].handler(client_socket, user, args);
        }
    }

    /* Invalid command handler */
    char resp[256];
    snprintf(resp, sizeof(resp), "[ERROR] Unknown command '%s'. Type /help for available commands.\n", cmd_name);
    sendToClient(client_socket, resp);
    writeLog(LOG_LEVEL_WARN, "Invalid command received on socket %d: %s", client_socket, cmd_name);
    return -1;
}

/* ==================== COMMAND HANDLERS ==================== */

static int cmd_register(int sock, User *user, const char *args) {
    if (user && user->online) {
        sendToClient(sock, "[ERROR] You are already logged in. Please /logout first.\n");
        return -1;
    }

    if (!args) {
        sendToClient(sock, "[USAGE] /register <username> <password>\n");
        return -1;
    }

    char username[MAX_USERNAME_LEN] = {0};
    char password[MAX_PASSWORD_LEN] = {0};

    if (sscanf(args, "%49s %49s", username, password) != 2) {
        sendToClient(sock, "[USAGE] /register <username> <password>\n");
        return -1;
    }

    char err[128];
    int new_id = registerUser(username, password, err, sizeof(err));
    if (new_id < 0) {
        char resp[256];
        snprintf(resp, sizeof(resp), "[ERROR] %s\n", err);
        sendToClient(sock, resp);
        return -1;
    }

    saveData();
    char resp[256];
    snprintf(resp, sizeof(resp), "[SUCCESS] Account '%s' registered successfully! You may now /login %s <password>\n", username, username);
    sendToClient(sock, resp);
    return 0;
}

static int cmd_login(int sock, User *user, const char *args) {
    if (user && user->online) {
        sendToClient(sock, "[ERROR] You are already logged in.\n");
        return -1;
    }

    if (!args) {
        sendToClient(sock, "[USAGE] /login <username> <password>\n");
        return -1;
    }

    char username[MAX_USERNAME_LEN] = {0};
    char password[MAX_PASSWORD_LEN] = {0};

    if (sscanf(args, "%49s %49s", username, password) != 2) {
        sendToClient(sock, "[USAGE] /login <username> <password>\n");
        return -1;
    }

    char err[128];
    User *logged_in = loginUser(sock, username, password, err, sizeof(err));
    if (!logged_in) {
        char resp[256];
        snprintf(resp, sizeof(resp), "[ERROR] %s\n", err);
        sendToClient(sock, resp);
        return -1;
    }

    ChatRoom *room = findRoomById(logged_in->room_id);
    const char *r_name = room ? room->name : "general";

    char resp[512];
    snprintf(resp, sizeof(resp),
             "[SUCCESS] Welcome back, %s! You joined #%s.\n"
             "Type /help for commands or start typing to chat.\n",
             logged_in->username, r_name);
    sendToClient(sock, resp);

    /* Broadcast join notification to room */
    char notify[256];
    snprintf(notify, sizeof(notify), "--> User '%s' has entered #%s.\n", logged_in->username, r_name);
    broadcastMessage(logged_in->room_id, notify, sock);

    return 0;
}

static int cmd_logout(int sock, User *user, const char *args) {
    (void)args;
    if (!user) return -1;

    char username[MAX_USERNAME_LEN];
    strncpy(username, user->username, sizeof(username) - 1);
    int room_id = user->room_id;

    logoutUser(user->id);

    char notify[256];
    snprintf(notify, sizeof(notify), "<-- User '%s' has logged out.\n", username);
    broadcastMessage(room_id, notify, sock);

    sendToClient(sock, "[SUCCESS] You have been logged out.\n");
    return 0;
}

static int cmd_users(int sock, User *user, const char *args) {
    (void)user;
    (void)args;
    char buffer[4096];
    listUsers(buffer, sizeof(buffer));
    sendToClient(sock, buffer);
    return 0;
}

static int cmd_rooms(int sock, User *user, const char *args) {
    (void)user;
    (void)args;
    char buffer[4096];
    listRooms(buffer, sizeof(buffer));
    sendToClient(sock, buffer);
    return 0;
}

static int cmd_join(int sock, User *user, const char *args) {
    if (!args || strlen(args) == 0) {
        sendToClient(sock, "[USAGE] /join <room_name>\n");
        return -1;
    }

    char target_room[MAX_ROOM_NAME_LEN];
    if (sscanf(args, "%49s", target_room) != 1) {
        sendToClient(sock, "[USAGE] /join <room_name>\n");
        return -1;
    }

    int old_room_id = user->room_id;
    char err[128];
    int new_room_id = joinRoom(user->id, target_room, err, sizeof(err));
    if (new_room_id < 0) {
        char resp[256];
        snprintf(resp, sizeof(resp), "[ERROR] %s\n", err);
        sendToClient(sock, resp);
        return -1;
    }

    ChatRoom *new_room = findRoomById(new_room_id);
    const char *new_name = new_room ? new_room->name : target_room;

    /* Notify old room */
    if (old_room_id != new_room_id) {
        char left_notify[256];
        snprintf(left_notify, sizeof(left_notify), "<-- User '%s' left this room.\n", user->username);
        broadcastMessage(old_room_id, left_notify, sock);
    }

    /* Notify new room */
    char enter_notify[256];
    snprintf(enter_notify, sizeof(enter_notify), "--> User '%s' joined #%s.\n", user->username, new_name);
    broadcastMessage(new_room_id, enter_notify, sock);

    char resp[256];
    snprintf(resp, sizeof(resp), "[SUCCESS] Switched to room #%s\n", new_name);
    sendToClient(sock, resp);

    saveData();
    return 0;
}

static int cmd_leave(int sock, User *user, const char *args) {
    (void)args;
    int old_room_id = user->room_id;
    char err[128];
    leaveRoom(user->id, err, sizeof(err));

    char left_notify[256];
    snprintf(left_notify, sizeof(left_notify), "<-- User '%s' left this room.\n", user->username);
    broadcastMessage(old_room_id, left_notify, sock);

    ChatRoom *curr_room = findRoomById(user->room_id);
    const char *r_name = curr_room ? curr_room->name : "general";

    char enter_notify[256];
    snprintf(enter_notify, sizeof(enter_notify), "--> User '%s' joined #%s.\n", user->username, r_name);
    broadcastMessage(user->room_id, enter_notify, sock);

    char resp[256];
    snprintf(resp, sizeof(resp), "[SUCCESS] Left room. Now in #%s\n", r_name);
    sendToClient(sock, resp);
    return 0;
}

static int cmd_msg(int sock, User *user, const char *args) {
    if (!args) {
        sendToClient(sock, "[USAGE] /msg <recipient_username> <message>\n");
        return -1;
    }

    char recipient[MAX_USERNAME_LEN] = {0};
    int offset = 0;
    if (sscanf(args, "%49s %n", recipient, &offset) != 1 || offset <= 0) {
        sendToClient(sock, "[USAGE] /msg <recipient_username> <message>\n");
        return -1;
    }

    const char *message_body = args + offset;
    while (*message_body == ' ') message_body++;

    if (strlen(message_body) == 0) {
        sendToClient(sock, "[ERROR] Message body cannot be empty.\n");
        return -1;
    }

    char err[128];
    if (sendPrivateMessage(user->id, recipient, message_body, err, sizeof(err)) != 0) {
        char resp[256];
        snprintf(resp, sizeof(resp), "[ERROR] %s\n", err);
        sendToClient(sock, resp);
        return -1;
    }

    saveData();
    return 0;
}

static int cmd_history(int sock, User *user, const char *args) {
    (void)args;
    char buffer[4096];
    showHistory(user->room_id, buffer, sizeof(buffer), 20);
    sendToClient(sock, buffer);
    return 0;
}

static int cmd_stats(int sock, User *user, const char *args) {
    (void)user;
    (void)args;
    char buffer[1024];
    formatStatsReport(buffer, sizeof(buffer), getOnlineUserCount(), getUserCount(), getRoomCount());
    sendToClient(sock, buffer);
    return 0;
}

static int cmd_help(int sock, User *user, const char *args) {
    (void)args;
    char buffer[2048];
    int is_admin = (user && (user->flags & USER_FLAG_ADMIN)) ? 1 : 0;
    getHelpText(buffer, sizeof(buffer), is_admin);
    sendToClient(sock, buffer);
    return 0;
}

static int cmd_quit(int sock, User *user, const char *args) {
    (void)args;
    sendToClient(sock, "[GOODBYE] Disconnecting. Thank you for using C Chat Server!\n");
    if (user) {
        char left_notify[256];
        snprintf(left_notify, sizeof(left_notify), "<-- User '%s' has disconnected.\n", user->username);
        broadcastMessage(user->room_id, left_notify, sock);
        disconnectUser(sock);
    }
    return 1; /* Signal to close socket */
}

static int cmd_kick(int sock, User *user, const char *args) {
    if (!user || !(user->flags & USER_FLAG_ADMIN)) {
        sendToClient(sock, "[ERROR] Permission denied: Administrator privileges required.\n");
        return -1;
    }

    if (!args) {
        sendToClient(sock, "[USAGE] /kick <username>\n");
        return -1;
    }

    char target_name[MAX_USERNAME_LEN];
    if (sscanf(args, "%49s", target_name) != 1) {
        sendToClient(sock, "[USAGE] /kick <username>\n");
        return -1;
    }

    User *target = findUserByUsername(target_name);
    if (!target || !target->online || target->socket_fd <= 0) {
        sendToClient(sock, "[ERROR] User is not online.\n");
        return -1;
    }

    sendToClient(target->socket_fd, "[KICKED] You have been kicked from the server by an administrator.\n");
    disconnectUser(target->socket_fd);
    close(target->socket_fd);

    char resp[256];
    snprintf(resp, sizeof(resp), "[SUCCESS] User '%s' has been kicked.\n", target_name);
    sendToClient(sock, resp);
    return 0;
}

static int cmd_ping(int sock, User *user, const char *args) {
    (void)user;
    (void)args;
    sendToClient(sock, "[PONG] latency_check_ok\n");
    return 0;
}

static int cmd_broadcast(int sock, User *user, const char *args) {
    if (!user || !(user->flags & USER_FLAG_ADMIN)) {
        sendToClient(sock, "[ERROR] Permission denied: Administrator privileges required for broadcast.\n");
        return -1;
    }

    if (!args || strlen(args) == 0) {
        sendToClient(sock, "[USAGE] /broadcast <announcement message>\n");
        return -1;
    }

    char time_str[10];
    get_short_time(time_str, sizeof(time_str));

    char announce[BUFFER_SIZE];
    snprintf(announce, sizeof(announce), "[%s] [GLOBAL ANNOUNCEMENT from %s]: %s\n",
             time_str, user->username, args);

    /* Broadcast to all connected users in all rooms */
    int total_users = getUserCount();
    for (int i = 0; i < total_users; i++) {
        User *u = getUserByIndex(i);
        if (u && u->online && u->socket_fd > 0) {
            sendToClient(u->socket_fd, announce);
        }
    }

    writeLog(LOG_LEVEL_INFO, "Admin '%s' sent global broadcast: %s", user->username, args);
    return 0;
}
