#ifndef LOGGER_H
#define LOGGER_H

#include "common.h"

/* Bitwise log levels */
#define LOG_LEVEL_INFO    (1 << 0)
#define LOG_LEVEL_WARN    (1 << 1)
#define LOG_LEVEL_ERROR   (1 << 2)
#define LOG_LEVEL_DEBUG   (1 << 3)

int initLogger(const char *log_filepath);
void closeLogger(void);

/* Section 9: writeLog() */
void writeLog(unsigned int level, const char *format, ...);

#endif /* LOGGER_H */
