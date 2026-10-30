#ifndef USER_H
#define USER_H

#include "common.h"

int initUserManager(void);
void cleanupUserManager(void);

/* Section 9 Required Functions */
int registerUser(const char *username, const char *password, char *err_msg, size_t err_len);
User* loginUser(int socket_fd, const char *username, const char *password, char *err_msg, size_t err_len);
int logoutUser(int user_id);
int disconnectUser(int socket_fd);
void listUsers(char *buffer, size_t size);

/* User Lookup & Search */
User* findUserById(int id);
User* findUserByUsername(const char *username);
User* findUserBySocket(int socket_fd);
int removeUser(int id);

/* Accessors & Utilities */
int getUserCount(void);
int getOnlineUserCount(void);
User* getUserByIndex(int index);
void sortUsersAlphabetically(User **sorted_array, int *count);

#endif /* USER_H */
