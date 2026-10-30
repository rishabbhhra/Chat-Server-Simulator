#ifndef ROOM_H
#define ROOM_H

#include "common.h"

int initRoomManager(void);
void cleanupRoomManager(void);

/* Section 9 Required Functions */
ChatRoom* createRoom(const char *name, char *err_msg, size_t err_len);
int joinRoom(int user_id, const char *room_name, char *err_msg, size_t err_len);
int leaveRoom(int user_id, char *err_msg, size_t err_len);

/* Room Lookup & Utilities */
ChatRoom* findRoomById(int room_id);
ChatRoom* findRoomByName(const char *name);
int getRoomCount(void);
ChatRoom* getRoomByIndex(int index);
void listRooms(char *buffer, size_t size);
void updateRoomUserCount(int room_id);

#endif /* ROOM_H */
