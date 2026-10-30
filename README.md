# Multi-Client Chat Server Simulator Using C

**Author:** Rishabh Raj  
**Technology:** C Standard Library (C99), POSIX Sockets, File Handling, `select()` I/O Multiplexing  

---

## 1. Overview
The **Multi-Client Chat Server Simulator** is a high-performance C systems application demonstrating how a centralized server manages multiple concurrent client connections, handles user authentication, routes public and private communications, manages chat rooms, maintains message histories, persists data to disk, and tracks server telemetry.

The project demonstrates modular C programming, advanced pointers, function pointer dispatch tables, dynamic memory allocation, POSIX socket multiplexing with `select()`, and file persistence.

---

## 2. System Architecture

```text
  ┌─────────────────────────────────────────────────────────┐
  │                       CHAT CLIENT                       │
  │   - Command/Input Layer (Terminal STDIN)                │
  │   - Dynamic Banner & Status UI                          │
  │   - Non-blocking Network Receiver via select()          │
  └────────────────────────────┬────────────────────────────┘
                               │ TCP POSIX Socket
                               ▼
  ┌─────────────────────────────────────────────────────────┐
  │                       CHAT SERVER                       │
  │   - Connection Manager (select() I/O Multiplexing)       │
  │   - Command Dispatcher (Function Pointer Table)         │
  └─────────────┬───────────────────────────┬───────────────┘
                │                           │
        ┌───────▼───────┐           ┌───────▼───────┐
        │  User Manager │           │ Room Manager  │
        └───────┬───────┘           └───────┬───────┘
                │                           │
                └─────────────┬─────────────┘
                              ▼
                      ┌───────────────┐
                      │Message Router │
                      └───────┬───────┘
                              │
                    ┌─────────▼─────────┐
                    │ File Persistence  │
                    │ & Logging Subsys  │
                    └───────────────────┘
```

---

## 3. Major Data Structures

### 3.1 User Structure (`include/common.h`)
```c
typedef struct {
    int id;
    char username[50];
    char password[50];
    int socket_fd;
    int room_id;
    int online;
    unsigned int flags; /* Bitwise status flags */
} User;
```

### 3.2 Chat Room Structure (`include/common.h`)
```c
typedef struct {
    int room_id;
    char name[50];
    int user_count;
} ChatRoom;
```

### 3.3 Message Structure (`include/common.h`)
```c
typedef struct {
    int sender_id;
    int receiver_id; /* -1 for public broadcast, or recipient user ID */
    char message[500];
    char timestamp[30];
    unsigned int flags; /* Bitwise message flags */
} Message;
```

---

## 4. Key Functions Implemented

| Function | Purpose | Location |
| :--- | :--- | :--- |
| `registerUser()` | Register a new unique user | [`src/user.c`](file:///Users/rishabh/Documents/Chat%20Server%20Simulator%20Using%20C/src/user.c) |
| `loginUser()` | Authenticate credentials and establish online session | [`src/user.c`](file:///Users/rishabh/Documents/Chat%20Server%20Simulator%20Using%20C/src/user.c) |
| `logoutUser()` | Log out current session | [`src/user.c`](file:///Users/rishabh/Documents/Chat%20Server%20Simulator%20Using%20C/src/user.c) |
| `sendMessage()` | Send public message to the current room | [`src/message.c`](file:///Users/rishabh/Documents/Chat%20Server%20Simulator%20Using%20C/src/message.c) |
| `sendPrivateMessage()` | Send private message to a specific user | [`src/message.c`](file:///Users/rishabh/Documents/Chat%20Server%20Simulator%20Using%20C/src/message.c) |
| `createRoom()` | Create an isolated chat room | [`src/room.c`](file:///Users/rishabh/Documents/Chat%20Server%20Simulator%20Using%20C/src/room.c) |
| `joinRoom()` | Move user to a target chat room | [`src/room.c`](file:///Users/rishabh/Documents/Chat%20Server%20Simulator%20Using%20C/src/room.c) |
| `leaveRoom()` | Leave current room back to `#general` | [`src/room.c`](file:///Users/rishabh/Documents/Chat%20Server%20Simulator%20Using%20C/src/room.c) |
| `listUsers()` | Display online users (alphabetically sorted) | [`src/user.c`](file:///Users/rishabh/Documents/Chat%20Server%20Simulator%20Using%20C/src/user.c) |
| `showHistory()` | Display stored message history | [`src/message.c`](file:///Users/rishabh/Documents/Chat%20Server%20Simulator%20Using%20C/src/message.c) |
| `handleCommand()` | Process user commands via function pointer dispatch | [`src/command.c`](file:///Users/rishabh/Documents/Chat%20Server%20Simulator%20Using%20C/src/command.c) |
| `broadcastMessage()` | Transmit messages across room members | [`src/message.c`](file:///Users/rishabh/Documents/Chat%20Server%20Simulator%20Using%20C/src/message.c) |
| `disconnectUser()` | Handle client disconnection and session cleanup | [`src/user.c`](file:///Users/rishabh/Documents/Chat%20Server%20Simulator%20Using%20C/src/user.c) |
| `saveData()` | Persist users, rooms, and history to disk files | [`src/storage.c`](file:///Users/rishabh/Documents/Chat%20Server%20Simulator%20Using%20C/src/storage.c) |
| `loadData()` | Load persisted records upon startup | [`src/storage.c`](file:///Users/rishabh/Documents/Chat%20Server%20Simulator%20Using%20C/src/storage.c) |
| `writeLog()` | Write timestamped messages to console and log file | [`src/logger.c`](file:///Users/rishabh/Documents/Chat%20Server%20Simulator%20Using%20C/src/logger.c) |

---

## 5. C Concepts Demonstrated

1. **Basic Concepts**: Variables, conditional statements, loops, functions, arrays, strings.
2. **Intermediate Concepts**: Structures, `typedef`, pointers, searching (linear & key lookup), sorting (`qsort` custom comparator for users), modular multi-file structure, file I/O (`fopen`, `fprintf`, `fgets`).
3. **Advanced Concepts**:
   - **Dynamic Memory Allocation**: Dynamic reallocation for users array and message history queue (`malloc`, `realloc`, `free`).
   - **Function Pointers**: Command dispatch table (`CommandHandler` function pointer type mapped to command strings).
   - **Command-Line Arguments**: Flags parsing for `--port`, `--data`, `--log`, `--server`.
   - **Socket & File Descriptors**: POSIX sockets, `select()` multiplexing for non-blocking I/O handling on single thread.
   - **Bitwise Operations**: Permission bitmasks (`USER_FLAG_ADMIN`, `USER_FLAG_ONLINE`), message flags (`MSG_FLAG_PUBLIC`, `MSG_FLAG_PRIVATE`), and logging levels (`LOG_LEVEL_INFO`, `LOG_LEVEL_WARN`, `LOG_LEVEL_ERROR`).

---

## 6. Build Instructions

To compile the entire suite (server, client, and test harness):
```bash
make
```

Binaries will be placed in `bin/`:
- `bin/chat_server`
- `bin/chat_client`
- `bin/test_suite`

To clean all build artifacts:
```bash
make clean
```

---

## 7. Running the Server and Client

### Starting the Server
```bash
./bin/chat_server -p 8080
```
Optional arguments:
- `-p <port>` : Port to bind (default: 8080)
- `-d <dir>`  : Storage directory (default: `data`)
- `-l <file>` : Log file location (default: `logs/server.log`)

### Starting the Client
In separate terminal windows, connect one or more clients:
```bash
./bin/chat_client -s 127.0.0.1 -p 8080
```

### Supported Commands

| Command | Usage | Description |
| :--- | :--- | :--- |
| `/register` | `/register <username> <password>` | Register a new user |
| `/login` | `/login <username> <password>` | Log into registered account |
| `/logout` | `/logout` | Log out from account |
| `/users` | `/users` | Display online users (alphabetical) |
| `/rooms` | `/rooms` | List active rooms and user counts |
| `/join` | `/join <room_name>` | Join or create a room (e.g., `/join coding`) |
| `/leave` | `/leave` | Leave room back to `#general` |
| `/msg` | `/msg <user> <message>` | Send private direct message |
| `/history` | `/history` | View recent messages in room |
| `/stats` | `/stats` | Display server statistics |
| `/help` | `/help` | List available commands |
| `/quit` | `/quit` | Exit chat client |
| `/kick` | `/kick <username>` | Administrator only: disconnect user |
| `<text>` | `Hello everyone!` | Plain text broadcasts to current room |

---

## 8. Automated Testing

Run the automated test suite covering all Normal, Boundary, and Invalid system test cases:
```bash
make test
```
Or run the live network simulation test:
```bash
python3 tests/live_integration_test.py
```
