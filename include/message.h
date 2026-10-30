#ifndef MESSAGE_H
#define MESSAGE_H

#include "common.h"

int initMessageManager(void);
void cleanupMessageManager(void);

/* Section 9 Required Functions */
int sendMessage(int sender_id, const char *text, char *err_msg, size_t err_len);
int sendPrivateMessage(int sender_id, const char *receiver_username, const char *text, char *err_msg, size_t err_len);
void broadcastMessage(int room_id, const char *text, int exclude_socket_fd);
void showHistory(int room_id, char *buffer, size_t size, int limit);

/* Message Storage and Retrieval */
int recordMessage(int sender_id, int receiver_id, const char *text, unsigned int flags);
int getMessageCount(void);
Message* getMessageByIndex(int index);

#endif /* MESSAGE_H */
