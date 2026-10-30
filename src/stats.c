#include "stats.h"

static ServerStats g_stats;

void initStats(void) {
    memset(&g_stats, 0, sizeof(ServerStats));
    g_stats.start_time = time(NULL);
}

ServerStats* getStats(void) {
    return &g_stats;
}

void recordConnection(void) {
    g_stats.total_connections++;
}

void recordDisconnection(void) {
    g_stats.total_disconnections++;
}

void recordRegistration(void) {
    g_stats.total_registrations++;
}

void recordLogin(void) {
    g_stats.total_logins++;
}

void recordPublicMessage(void) {
    g_stats.total_public_messages++;
}

void recordPrivateMessage(void) {
    g_stats.total_private_messages++;
}

void recordCommand(void) {
    g_stats.total_commands++;
}

void formatStatsReport(char *buffer, size_t size, int active_users, int total_users, int total_rooms) {
    time_t now = time(NULL);
    long uptime = (long)difftime(now, g_stats.start_time);
    long hours = uptime / 3600;
    long minutes = (uptime % 3600) / 60;
    long seconds = uptime % 60;

    snprintf(buffer, size,
        "================ SERVER STATISTICS ================\n"
        " Uptime:                 %02ldh %02ldm %02lds\n"
        " Active Online Users:    %d\n"
        " Total Registered Users: %d\n"
        " Active Chat Rooms:      %d\n"
        " Total Connections:      %d\n"
        " Total Disconnections:   %d\n"
        " Total User Logins:      %d\n"
        " Total Public Messages:  %d\n"
        " Total Private Messages: %d\n"
        " Total Commands Handled: %d\n"
        "===================================================\n",
        hours, minutes, seconds,
        active_users,
        total_users,
        total_rooms,
        g_stats.total_connections,
        g_stats.total_disconnections,
        g_stats.total_logins,
        g_stats.total_public_messages,
        g_stats.total_private_messages,
        g_stats.total_commands
    );
}
