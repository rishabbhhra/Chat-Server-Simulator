#include "server.h"

static void printUsage(const char *prog_name) {
    printf("Multi-Client Chat Server Simulator\n");
    printf("Author: Rishabh Raj\n\n");
    printf("Usage: %s [OPTIONS]\n", prog_name);
    printf("Options:\n");
    printf("  -p, --port <port>       Port to listen on (default: %d)\n", DEFAULT_PORT);
    printf("  -d, --data <dir>        Directory to store user and message data (default: %s)\n", DEFAULT_DATA_DIR);
    printf("  -l, --log <file>        Path to server activity log file (default: %s/server.log)\n", DEFAULT_LOG_DIR);
    printf("  -h, --help              Show this help message and exit\n");
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    const char *data_dir = DEFAULT_DATA_DIR;
    char log_file[256];
    snprintf(log_file, sizeof(log_file), "%s/server.log", DEFAULT_LOG_DIR);

    /* Command-line argument parsing (Demonstrating Advanced Concept) */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
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
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--data") == 0) {
            if (i + 1 < argc) {
                data_dir = argv[++i];
            } else {
                fprintf(stderr, "Error: Missing argument for %s\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--log") == 0) {
            if (i + 1 < argc) {
                strncpy(log_file, argv[++i], sizeof(log_file) - 1);
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

    /* Initialize and start server */
    if (initServer(port, data_dir, log_file) != 0) {
        fprintf(stderr, "Failed to initialize server on port %d.\n", port);
        return 1;
    }

    return runServer();
}
