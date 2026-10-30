CC = clang
CFLAGS = -Wall -Wextra -O2 -Iinclude -std=c99
LDFLAGS = 

# Directories
SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj
BIN_DIR = bin
TEST_DIR = tests
DATA_DIR = data
LOG_DIR = logs

# Source files common to server & tests
CORE_SRCS = $(SRC_DIR)/user.c \
            $(SRC_DIR)/room.c \
            $(SRC_DIR)/message.c \
            $(SRC_DIR)/storage.c \
            $(SRC_DIR)/command.c \
            $(SRC_DIR)/logger.c \
            $(SRC_DIR)/stats.c

SERVER_SRCS = $(CORE_SRCS) $(SRC_DIR)/server.c $(SRC_DIR)/server_main.c
CLIENT_SRCS = $(SRC_DIR)/client.c $(SRC_DIR)/client_main.c
TEST_SRCS   = $(CORE_SRCS) $(TEST_DIR)/test_suite.c $(SRC_DIR)/server.c

# Object files
CORE_OBJS   = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(CORE_SRCS))
SERVER_OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SERVER_SRCS))
CLIENT_OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(CLIENT_SRCS))
TEST_OBJS   = $(CORE_OBJS) $(OBJ_DIR)/server.o $(OBJ_DIR)/test_suite.o

# Binaries
SERVER_BIN = $(BIN_DIR)/chat_server
CLIENT_BIN = $(BIN_DIR)/chat_client
TEST_BIN   = $(BIN_DIR)/test_suite

.PHONY: all clean test run_server run_client dirs

all: dirs $(SERVER_BIN) $(CLIENT_BIN) $(TEST_BIN)

dirs:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR) $(DATA_DIR) $(LOG_DIR)

# Pattern rule for src objects
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Pattern rule for test objects
$(OBJ_DIR)/test_suite.o: $(TEST_DIR)/test_suite.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Executables
$(SERVER_BIN): $(SERVER_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(SERVER_OBJS) $(LDFLAGS) -o $@
	@echo "Built: $@"

$(CLIENT_BIN): $(CLIENT_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CLIENT_OBJS) $(LDFLAGS) -o $@
	@echo "Built: $@"

$(TEST_BIN): $(TEST_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(TEST_OBJS) $(LDFLAGS) -o $@
	@echo "Built: $@"

test: $(TEST_BIN)
	@echo "Running Comprehensive Automated Tests..."
	@./$(TEST_BIN)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) test_data test_logs
	@echo "Clean completed."
