#include "common.h"
#include "user.h"
#include "room.h"
#include "message.h"
#include "command.h"
#include "storage.h"
#include "stats.h"
#include "logger.h"
#include <assert.h>
#include <sys/stat.h>

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, desc) do { \
    if (cond) { \
        printf("  [PASS] %s\n", desc); \
        g_tests_passed++; \
    } else { \
        printf("  [FAIL] %s\n", desc); \
        g_tests_failed++; \
    } \
} while (0)

void setup_test_environment(void) {
    mkdir("test_data", 0755);
    mkdir("test_logs", 0755);
    initLogger("test_logs/test.log");
    initStats();
    initUserManager();
    initRoomManager();
    initMessageManager();
    initStorage("test_data");
    initCommandSystem();
}

void teardown_test_environment(void) {
    cleanupMessageManager();
    cleanupRoomManager();
    cleanupUserManager();
    closeLogger();
}

/* 13.1 Normal Test Cases */
void test_normal_cases(void) {
    printf("\n=== RUNNING NORMAL TEST CASES ===\n");
    char err[128];

    /* 1. Successful registration */
    int user1_id = registerUser("alice", "pass123", err, sizeof(err));
    TEST_ASSERT(user1_id > 0, "Normal 1: Successful registration for user 'alice'");

    int user2_id = registerUser("bob", "secret456", err, sizeof(err));
    TEST_ASSERT(user2_id > 0, "Normal 1b: Successful registration for user 'bob'");

    /* 2. Successful login */
    User *u1 = loginUser(101, "alice", "pass123", err, sizeof(err));
    TEST_ASSERT(u1 != NULL && u1->online == 1, "Normal 2: Successful login for 'alice'");

    User *u2 = loginUser(102, "bob", "secret456", err, sizeof(err));
    TEST_ASSERT(u2 != NULL && u2->online == 1, "Normal 2b: Successful login for 'bob'");

    /* 3. Sending a valid message */
    int msg_res = sendMessage(u1->id, "Hello everyone in coding!", err, sizeof(err));
    TEST_ASSERT(msg_res == 0, "Normal 3: Sending a valid public message");

    /* 4. Joining an existing room */
    int join_res = joinRoom(u1->id, "coding", err, sizeof(err));
    TEST_ASSERT(join_res > 0 && u1->room_id == join_res, "Normal 4: Joining an existing room (#coding)");

    /* 5. Sending a private message */
    int pm_res = sendPrivateMessage(u1->id, "bob", "Hi Bob, check your email", err, sizeof(err));
    TEST_ASSERT(pm_res == 0, "Normal 5: Sending a private message to 'bob'");
}

/* 13.2 Boundary Test Cases */
void test_boundary_cases(void) {
    printf("\n=== RUNNING BOUNDARY TEST CASES ===\n");
    char err[128];

    /* 1. Long usernames (exact max length boundary) */
    char max_user[MAX_USERNAME_LEN];
    memset(max_user, 'u', MAX_USERNAME_LEN - 1);
    max_user[MAX_USERNAME_LEN - 1] = '\0';
    int long_res = registerUser(max_user, "password", err, sizeof(err));
    TEST_ASSERT(long_res > 0, "Boundary 1: Maximum length username accepted");

    /* 2. Maximum message length boundary */
    User *u1 = findUserByUsername("alice");
    char max_msg[MAX_MESSAGE_LEN];
    memset(max_msg, 'a', MAX_MESSAGE_LEN - 1);
    max_msg[MAX_MESSAGE_LEN - 1] = '\0';
    int max_msg_res = sendMessage(u1->id, max_msg, err, sizeof(err));
    TEST_ASSERT(max_msg_res == 0, "Boundary 2: Exact maximum message length handled");

    /* Oversized message boundary check */
    char over_msg[MAX_MESSAGE_LEN + 50];
    memset(over_msg, 'x', sizeof(over_msg) - 1);
    over_msg[sizeof(over_msg) - 1] = '\0';
    int over_res = sendMessage(u1->id, over_msg, err, sizeof(err));
    TEST_ASSERT(over_res != 0, "Boundary 2b: Message exceeding maximum length rejected");

    /* 3. Maximum number of rooms */
    int initial_rooms = getRoomCount();
    for (int i = initial_rooms; i < MAX_ROOMS; i++) {
        char r_name[32];
        snprintf(r_name, sizeof(r_name), "room_%d", i);
        createRoom(r_name, err, sizeof(err));
    }
    TEST_ASSERT(getRoomCount() == MAX_ROOMS, "Boundary 3: Reached maximum number of rooms");
    ChatRoom *overflow_room = createRoom("room_overflow", err, sizeof(err));
    TEST_ASSERT(overflow_room == NULL, "Boundary 3b: Creating room past MAX_ROOMS rejected");

    /* 4. Empty chat room */
    ChatRoom *empty_r = findRoomByName("gaming");
    TEST_ASSERT(empty_r != NULL && empty_r->user_count == 0, "Boundary 4: Empty chat room verified with 0 active users");

    /* 5. Online User sorting & listing with multiple users */
    char users_list[2048];
    listUsers(users_list, sizeof(users_list));
    TEST_ASSERT(strstr(users_list, "alice") != NULL && strstr(users_list, "bob") != NULL,
                "Boundary 5: User list output contains active users properly formatted");
}

/* 13.3 Invalid Test Cases */
void test_invalid_cases(void) {
    printf("\n=== RUNNING INVALID TEST CASES ===\n");
    char err[128];

    /* 1. Duplicate username */
    int dup_res = registerUser("alice", "anotherpass", err, sizeof(err));
    TEST_ASSERT(dup_res < 0, "Invalid 1: Duplicate username 'alice' correctly rejected");

    /* 2. Incorrect password */
    User *wrong_pw = loginUser(201, "alice", "wrong_password", err, sizeof(err));
    TEST_ASSERT(wrong_pw == NULL, "Invalid 2: Incorrect password login correctly rejected");

    /* 3. Invalid command */
    int inv_cmd = handleCommand(101, "/nonexistentcommand foo bar");
    TEST_ASSERT(inv_cmd < 0, "Invalid 3: Unrecognized command properly rejected");

    /* 4. Non-existent user */
    User *u_alice = findUserByUsername("alice");
    int pm_ghost = sendPrivateMessage(u_alice->id, "ghost_user", "You there?", err, sizeof(err));
    TEST_ASSERT(pm_ghost < 0, "Invalid 4: Sending private message to non-existent user fails");

    /* 5. Non-existent room (should handle properly or auto-create/reject) */
    ChatRoom *non_room = findRoomByName("mars_station_9999");
    TEST_ASSERT(non_room == NULL, "Invalid 5: Non-existent room search returns NULL");

    /* 6. Empty message */
    int empty_msg = sendMessage(u_alice->id, "", err, sizeof(err));
    TEST_ASSERT(empty_msg < 0, "Invalid 6: Empty message correctly rejected");

    /* 7. Disconnected client */
    int disc_id = disconnectUser(101);
    TEST_ASSERT(disc_id == u_alice->id && u_alice->online == 0, "Invalid 7: Disconnected client handled and marked offline");
}

/* 13.4 Persistence and History Tests */
void test_storage_and_history(void) {
    printf("\n=== RUNNING PERSISTENCE & HISTORY TESTS ===\n");
    int save_res = saveData();
    TEST_ASSERT(save_res == 0, "Storage 1: saveData() writes users, rooms, and history successfully");

    char hist_buf[2048];
    showHistory(-1, hist_buf, sizeof(hist_buf), 10);
    TEST_ASSERT(strlen(hist_buf) > 0 && strstr(hist_buf, "MESSAGE HISTORY") != NULL,
                "History 1: showHistory() retrieves recorded message log");
}

int main(void) {
    printf("====================================================\n");
    printf("   MULTI-CLIENT CHAT SERVER SIMULATOR TEST SUITE   \n");
    printf("   Comprehensive Systems & Network Tests            \n");
    printf("====================================================\n");

    setup_test_environment();

    test_normal_cases();
    test_boundary_cases();
    test_invalid_cases();
    test_storage_and_history();

    teardown_test_environment();

    printf("\n====================================================\n");
    printf("   TEST RESULTS: %d PASSED, %d FAILED               \n", g_tests_passed, g_tests_failed);
    printf("====================================================\n");

    return (g_tests_failed == 0) ? 0 : 1;
}
