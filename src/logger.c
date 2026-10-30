#include "logger.h"
#include <stdarg.h>

static FILE *log_file_ptr = NULL;

int initLogger(const char *log_filepath) {
    if (!log_filepath) {
        return -1;
    }
    log_file_ptr = fopen(log_filepath, "a");
    if (!log_file_ptr) {
        perror("Failed to open log file");
        return -1;
    }
    writeLog(LOG_LEVEL_INFO, "=== Chat Server Logging Started ===");
    return 0;
}

void closeLogger(void) {
    if (log_file_ptr) {
        writeLog(LOG_LEVEL_INFO, "=== Chat Server Logging Stopped ===");
        fclose(log_file_ptr);
        log_file_ptr = NULL;
    }
}

/* Section 9: writeLog() */
void writeLog(unsigned int level, const char *format, ...) {
    char timestamp[MAX_TIMESTAMP_LEN];
    get_current_timestamp(timestamp, sizeof(timestamp));

    const char *level_str = "UNKNOWN";
    /* Bitwise comparison of log levels */
    if (level & LOG_LEVEL_ERROR) {
        level_str = "ERROR";
    } else if (level & LOG_LEVEL_WARN) {
        level_str = "WARN";
    } else if (level & LOG_LEVEL_INFO) {
        level_str = "INFO";
    } else if (level & LOG_LEVEL_DEBUG) {
        level_str = "DEBUG";
    }

    char message_buf[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message_buf, sizeof(message_buf), format, args);
    va_end(args);

    /* Print to stdout/stderr */
    if (level & LOG_LEVEL_ERROR) {
        fprintf(stderr, "[%s] [%s] %s\n", timestamp, level_str, message_buf);
    } else {
        printf("[%s] [%s] %s\n", timestamp, level_str, message_buf);
    }

    /* Print to log file if open */
    if (log_file_ptr) {
        fprintf(log_file_ptr, "[%s] [%s] %s\n", timestamp, level_str, message_buf);
        fflush(log_file_ptr);
    }
}
