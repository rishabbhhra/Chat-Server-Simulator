#ifndef STATS_H
#define STATS_H

#include "common.h"

typedef struct {
    time_t start_time;
    int total_connections;
    int total_registrations;
    int total_logins;
    int total_public_messages;
    int total_private_messages;
    int total_commands;
    int total_disconnections;
} ServerStats;

void initStats(void);
ServerStats* getStats(void);
void recordConnection(void);
void recordDisconnection(void);
void recordRegistration(void);
void recordLogin(void);
void recordPublicMessage(void);
void recordPrivateMessage(void);
void recordCommand(void);
void formatStatsReport(char *buffer, size_t size, int active_users, int total_users, int total_rooms);

#endif /* STATS_H */
