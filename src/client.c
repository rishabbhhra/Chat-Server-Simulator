#include "client.h"
#include <signal.h>
#include <sys/select.h>

static ClientConfig g_client;

static void handleClientSignal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\nDisconnecting from chat server...\n");
        g_client.running = 0;
    }
}

void renderClientBanner(const char *username, const char *status, const char *room) {
    printf("\n");
    printf("=========================================\n");
    printf(" C CHAT CLIENT                           \n");
    printf("=========================================\n");
    printf("Username: %s\n", (username && strlen(username) > 0) ? username : "(Not Logged In)");
    printf("Status:   %s\n", status ? status : "OFFLINE");
    printf("Room:     #%s\n", (room && strlen(room) > 0) ? room : "general");
    printf("Available Commands:\n");
    printf("-----------------------------------------\n");
    printf("/users\n");
    printf("/join <room>\n");
    printf("/msg <user> <message>\n");
    printf("/history\n");
    printf("/help\n");
    printf("/quit\n");
    printf("-----------------------------------------\n\n");
    fflush(stdout);
}

int initClient(const char *server_ip, int port) {
    memset(&g_client, 0, sizeof(ClientConfig));
    strncpy(g_client.server_ip, (server_ip && strlen(server_ip) > 0) ? server_ip : DEFAULT_IP, sizeof(g_client.server_ip) - 1);
    g_client.server_port = (port > 0) ? port : DEFAULT_PORT;
    g_client.socket_fd = -1;
    g_client.running = 0;
    g_client.logged_in = 0;
    strcpy(g_client.current_room, "general");
    strcpy(g_client.username, "");

    signal(SIGINT, handleClientSignal);
    signal(SIGTERM, handleClientSignal);

    /* 1. Create socket */
    g_client.socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_client.socket_fd < 0) {
        perror("Failed to create socket");
        return -1;
    }

    /* 2. Connect to server */
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(g_client.server_port);

    if (inet_pton(AF_INET, g_client.server_ip, &serv_addr.sin_addr) <= 0) {
        perror("Invalid server address");
        close(g_client.socket_fd);
        g_client.socket_fd = -1;
        return -1;
    }

    if (connect(g_client.socket_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection to server failed");
        close(g_client.socket_fd);
        g_client.socket_fd = -1;
        return -1;
    }

    return 0;
}

int runClient(void) {
    if (g_client.socket_fd < 0) return -1;
    g_client.running = 1;

    renderClientBanner(g_client.username, "CONNECTING...", g_client.current_room);

    fd_set read_fds;
    char in_buf[BUFFER_SIZE];
    char out_buf[BUFFER_SIZE];

    while (g_client.running) {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(g_client.socket_fd, &read_fds);

        int max_fd = (g_client.socket_fd > STDIN_FILENO) ? g_client.socket_fd : STDIN_FILENO;

        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);

        if (activity < 0) {
            if (errno == EINTR) continue;
            perror("select error");
            break;
        }

        /* 1. Incoming messages from Server */
        if (FD_ISSET(g_client.socket_fd, &read_fds)) {
            memset(in_buf, 0, sizeof(in_buf));
            ssize_t bytes_recv = recv(g_client.socket_fd, in_buf, sizeof(in_buf) - 1, 0);

            if (bytes_recv <= 0) {
                printf("\n[Disconnected from server]\n");
                break;
            }

            in_buf[bytes_recv] = '\0';
            printf("%s", in_buf);
            fflush(stdout);

            /* Check if server response indicates login or room change */
            char parsed_user[MAX_USERNAME_LEN];
            char parsed_room[MAX_ROOM_NAME_LEN];
            if (sscanf(in_buf, "[SUCCESS] Welcome back, %49[^!]! You joined #%49[^.]", parsed_user, parsed_room) == 2) {
                strncpy(g_client.username, parsed_user, sizeof(g_client.username) - 1);
                strncpy(g_client.current_room, parsed_room, sizeof(g_client.current_room) - 1);
                g_client.logged_in = 1;
                renderClientBanner(g_client.username, "ONLINE", g_client.current_room);
            } else if (sscanf(in_buf, "[SUCCESS] Switched to room #%49s", parsed_room) == 1) {
                strncpy(g_client.current_room, parsed_room, sizeof(g_client.current_room) - 1);
            } else if (strstr(in_buf, "[SUCCESS] You have been logged out.")) {
                g_client.logged_in = 0;
                strcpy(g_client.username, "");
                renderClientBanner("", "OFFLINE", "general");
            }
        }

        /* 2. User Input from terminal stdin */
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            memset(out_buf, 0, sizeof(out_buf));
            if (fgets(out_buf, sizeof(out_buf), stdin) == NULL) {
                /* End of input (Ctrl+D) */
                break;
            }

            trim_newline(out_buf);
            if (strlen(out_buf) == 0) continue;

            /* Send command or message to server */
            strcat(out_buf, "\n");
            if (send(g_client.socket_fd, out_buf, strlen(out_buf), 0) < 0) {
                perror("Failed to send message");
                break;
            }

            if (strncmp(out_buf, "/quit", 5) == 0) {
                break;
            }
        }
    }

    if (g_client.socket_fd >= 0) {
        close(g_client.socket_fd);
        g_client.socket_fd = -1;
    }
    printf("Session ended.\n");
    return 0;
}
