#include "client.h"

static void printUsage(const char *prog_name) {
    printf("Multi-Client Chat Client Simulator\n");
    printf("Author: Rishabh Raj\n\n");
    printf("Usage: %s [OPTIONS]\n", prog_name);
    printf("Options:\n");
    printf("  -s, --server <ip>       Server IP address (default: %s)\n", DEFAULT_IP);
    printf("  -p, --port <port>       Server port number (default: %d)\n", DEFAULT_PORT);
    printf("  -h, --help              Show this help message and exit\n");
}

int main(int argc, char *argv[]) {
    const char *server_ip = DEFAULT_IP;
    int port = DEFAULT_PORT;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--server") == 0) {
            if (i + 1 < argc) {
                server_ip = argv[++i];
            } else {
                fprintf(stderr, "Error: Missing argument for %s\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
            if (i + 1 < argc) {
                port = atoi(argv[++i]);
                if (port <= 0 || port > 65535) {
                    fprintf(stderr, "Error: Invalid port number '%s'. Must be 1-65535.\n", argv[i]);
                    return 1;
                }
            } else {
                fprintf(stderr, "Error: Missing argument for %s\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
            printUsage(argv[0]);
            return 1;
        }
    }

    if (initClient(server_ip, port) != 0) {
        fprintf(stderr, "Could not connect to Chat Server at %s:%d.\n", server_ip, port);
        return 1;
    }

    return runClient();
}
