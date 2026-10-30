#include "server.h"
#include "user.h"
#include "room.h"
#include "message.h"
#include "command.h"
#include "storage.h"
#include "logger.h"
#include "stats.h"
#include <signal.h>
#include <sys/select.h>

static ServerConfig g_server;

static void handleSignal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        writeLog(LOG_LEVEL_INFO, "Termination signal received (%d). Shutting down server...", sig);
        g_server.running = 0;
    }
}

int initServer(int port, const char *data_dir, const char *log_file) {
    memset(&g_server, 0, sizeof(ServerConfig));
    g_server.port = (port > 0) ? port : DEFAULT_PORT;
    g_server.max_clients = MAX_CLIENTS;
    g_server.running = 0;

    if (data_dir) {
        strncpy(g_server.data_dir, data_dir, sizeof(g_server.data_dir) - 1);
    } else {
        strcpy(g_server.data_dir, DEFAULT_DATA_DIR);
    }

    if (log_file) {
        strncpy(g_server.log_file, log_file, sizeof(g_server.log_file) - 1);
    } else {
        snprintf(g_server.log_file, sizeof(g_server.log_file), "%s/server.log", DEFAULT_LOG_DIR);
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        g_server.client_sockets[i] = -1;
    }

    /* Initialize subsystems */
    initLogger(g_server.log_file);
    initStats();
    initUserManager();
    initRoomManager();
    initMessageManager();
    initStorage(g_server.data_dir);
    initCommandSystem();

    /* Load persisted records */
    loadData();

    /* Setup signal handler */
    signal(SIGINT, handleSignal);
    signal(SIGTERM, handleSignal);

    /* 1. Create Server Socket */
    g_server.server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server.server_socket < 0) {
        writeLog(LOG_LEVEL_ERROR, "Socket creation failed: %s", strerror(errno));
        return -1;
    }

    /* Enable SO_REUSEADDR */
    int opt = 1;
    if (setsockopt(g_server.server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        writeLog(LOG_LEVEL_WARN, "setsockopt SO_REUSEADDR failed: %s", strerror(errno));
    }

    /* 2. Bind the server to port */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(g_server.port);

    if (bind(g_server.server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        writeLog(LOG_LEVEL_ERROR, "Bind failed on port %d: %s", g_server.port, strerror(errno));
        close(g_server.server_socket);
        g_server.server_socket = -1;
        return -1;
    }

    /* 3. Listen for connections */
    if (listen(g_server.server_socket, 10) < 0) {
        writeLog(LOG_LEVEL_ERROR, "Listen failed: %s", strerror(errno));
        close(g_server.server_socket);
        g_server.server_socket = -1;
        return -1;
    }

    writeLog(LOG_LEVEL_INFO, "Chat Server successfully listening on port %d", g_server.port);
    return 0;
}

int sendToClient(int client_socket, const char *message) {
    if (client_socket <= 0 || !message) return -1;
    size_t len = strlen(message);
    ssize_t sent = send(client_socket, message, len, 0);
    if (sent < 0) {
        writeLog(LOG_LEVEL_WARN, "Failed to send data to socket %d: %s", client_socket, strerror(errno));
        return -1;
    }
    return 0;
}

int runServer(void) {
    if (g_server.server_socket < 0) {
        return -1;
    }

    g_server.running = 1;
    fd_set read_fds;
    char buffer[BUFFER_SIZE];

    printf("\n=======================================================\n");
    printf("   MULTI-CLIENT CHAT SERVER SIMULATOR                  \n");
    printf("   Status: RUNNING on Port %d                          \n", g_server.port);
    printf("   Waiting for incoming connections... (Press Ctrl+C to stop)\n");
    printf("=======================================================\n\n");

    while (g_server.running) {
        FD_ZERO(&read_fds);
        FD_SET(g_server.server_socket, &read_fds);
        int max_fd = g_server.server_socket;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = g_server.client_sockets[i];
            if (sd > 0) {
                FD_SET(sd, &read_fds);
            }
            if (sd > max_fd) {
                max_fd = sd;
            }
        }

        /* Timeout of 1 second for responsive signal handling */
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &tv);

        if (activity < 0 && errno != EINTR) {
            writeLog(LOG_LEVEL_ERROR, "select() error: %s", strerror(errno));
            break;
        }

        if (!g_server.running) {
            break;
        }

        /* 4. Check for new incoming connection on server socket */
        if (FD_ISSET(g_server.server_socket, &read_fds)) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int new_socket = accept(g_server.server_socket, (struct sockaddr *)&client_addr, &addr_len);

            if (new_socket < 0) {
                writeLog(LOG_LEVEL_WARN, "accept() failed: %s", strerror(errno));
            } else {
                char client_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
                int client_port = ntohs(client_addr.sin_port);

                writeLog(LOG_LEVEL_INFO, "New connection accepted from %s:%d (socket fd %d)",
                         client_ip, client_port, new_socket);
                recordConnection();

                /* Add new socket to list of client sockets */
                int added = 0;
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (g_server.client_sockets[i] == -1) {
                        g_server.client_sockets[i] = new_socket;
                        added = 1;
                        break;
                    }
                }

                if (!added) {
                    /* Max clients boundary condition */
                    writeLog(LOG_LEVEL_WARN, "Server full: rejecting connection from %s:%d", client_ip, client_port);
                    sendToClient(new_socket, "[ERROR] Server is currently at maximum capacity. Please try again later.\n");
                    close(new_socket);
                } else {
                    /* Send welcome greeting and prompt */
                    char welcome[BUFFER_SIZE];
                    snprintf(welcome, sizeof(welcome),
                        "=====================================================\n"
                        " Welcome to the Multi-Client Chat Server Simulator!  \n"
                        "-----------------------------------------------------\n"
                        " Please authenticate to begin:                       \n"
                        "   /login <username> <password>                      \n"
                        "   /register <username> <password>                   \n"
                        "   /help                                             \n"
                        "=====================================================\n");
                    sendToClient(new_socket, welcome);
                }
            }
        }

        /* 5. Check all client sockets for incoming data */
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = g_server.client_sockets[i];
            if (sd > 0 && FD_ISSET(sd, &read_fds)) {
                memset(buffer, 0, sizeof(buffer));
                ssize_t bytes_read = read(sd, buffer, sizeof(buffer) - 1);

                if (bytes_read <= 0) {
                    /* Client disconnected or error */
                    User *u = findUserBySocket(sd);
                    if (u) {
                        char left_notify[256];
                        snprintf(left_notify, sizeof(left_notify), "<-- User '%s' has disconnected.\n", u->username);
                        broadcastMessage(u->room_id, left_notify, sd);
                    }
                    disconnectUser(sd);
                    close(sd);
                    g_server.client_sockets[i] = -1;
                } else {
                    buffer[bytes_read] = '\0';
                    int result = handleCommand(sd, buffer);
                    if (result == 1) {
                        /* /quit command requested disconnection */
                        close(sd);
                        g_server.client_sockets[i] = -1;
                    }
                }
            }
        }
    }

    stopServer();
    return 0;
}

void stopServer(void) {
    writeLog(LOG_LEVEL_INFO, "Shutting down Chat Server and saving all state...");

    /* Notify and disconnect all connected clients */
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_server.client_sockets[i] > 0) {
            sendToClient(g_server.client_sockets[i], "[SERVER SHUTDOWN] The chat server is shutting down. Goodbye.\n");
            close(g_server.client_sockets[i]);
            g_server.client_sockets[i] = -1;
        }
    }

    /* Close listening socket */
    if (g_server.server_socket >= 0) {
        close(g_server.server_socket);
        g_server.server_socket = -1;
    }

    /* Save persistent data */
    saveData();

    /* Clean up subsystems */
    cleanupMessageManager();
    cleanupRoomManager();
    cleanupUserManager();
    closeLogger();

    printf("\nChat Server cleanly stopped.\n");
}
