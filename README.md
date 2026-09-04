# Distributed Chat Server

C++17 chat system with a TCP server, a console client, username/password authentication, and a pipe-delimited text database. The long-term design is multiple chat servers that stay in sync and share one store. This repository currently implements a **single Windows server** (Winsock) plus a client whose network layer is still stubbed.

## What it does

- **Server** (`chat_server`) initializes Winsock, binds TCP port `5555`, and owns a worker thread pool, connection manager, and packet router.
- **Client** (`chat_client`) assigns a local id, “connects” (stub), and shows a Login / Register / Exit menu.
- **Packets** carry typed requests: login, logout, chat message, join room, leave room.
- **Authentication** registers and logs users against `data/db.txt`.
- **Persistence** stores users, rooms, and messages in three sections of that file.

Several pieces are sketched but not fully wired: accept loop and session I/O, real client sockets, room broadcast, heartbeat, inter-server sync, and WebSocket/desktop UIs.

## Target architecture

```
                    +------------------+
                    |   Desktop/Web    |
                    |      Client      |
                    +--------+---------+
                             |
                         TCP / WebSocket
                             |
          +------------------+------------------+
          |                                     |
   +------+-------+                     +--------+------+
   | Chat Server 1|<------Sync--------->| Chat Server 2 |
   +------+-------+                     +--------+------+
          |                                     |
          +------------------+------------------+
                             |
                    +--------+--------+
                    |    Database      |
                    +------------------+
```

**Today:** one server process, console client, local text DB. Sync between servers is not implemented.

## Repository layout

```
Distributed-Chat-Server/
├── CMakeLists.txt          Build: utils, server_lib, client_lib
├── compile_commands.json   Compile database for IDEs
├── README.md               This file
├── data/db.txt             Runtime text database (also copied under src/data)
├── include/                Public headers (mirrors src/)
│   ├── auth/
│   ├── client/
│   ├── config/config.h     Port, thread count, DB path
│   ├── server/
│   └── utils/models/
├── src/
│   ├── run.ps1             Configure, build, and run chat_server
│   ├── auth/               AUTH.md
│   ├── client/             CLIENT.md
│   ├── data/               DATA.md
│   ├── server/             SERVER.md
│   └── utils/              UTILS.md  →  models/ MODELS.md
├── scripts/                Extra build/run helpers (stubs)
└── build/                  CMake output (generated)
```

Headers live in `include/`; implementations live in `src/`. CMake adds `include/` as a public include path.

| Folder docs | Covers |
|-------------|--------|
| [src/server/SERVER.md](src/server/SERVER.md) | Listen socket, sessions, packet routing, managers, thread pool |
| [src/client/CLIENT.md](src/client/CLIENT.md) | Console UI, packet build/handle, network stub |
| [src/auth/AUTH.md](src/auth/AUTH.md) | Register, login, password check |
| [src/utils/UTILS.md](src/utils/UTILS.md) | Shared `utils` library |
| [src/utils/models/MODELS.md](src/utils/models/MODELS.md) | User, Room, Message, Packet, Logger |
| [src/data/DATA.md](src/data/DATA.md) | Text database format |

## How a request is meant to flow

```
Client                         Server
  |                              |
  |  Packet (LOGIN, MESSAGE, …)  |
  |----------------------------->|
  |                    PacketProcessor
  |                         ├── Authentication
  |                         ├── UserManager
  |                         ├── RoomManager
  |                         └── MessageManager
  |                                    └── DatabaseManager → data/db.txt
  |  Packet (response)               |
  |<-----------------------------|
```

`PacketProcessor` already routes those types and calls `Authentication` for login/logout. The server does **not** yet accept clients or read sockets, so this path is not live end-to-end.

## Build and run

Requires CMake 3.16+, a C++17 compiler, and **Ninja** if you use `src/run.ps1`. The server links **Winsock** (`ws2_32`).

```powershell
# Configure and build everything
cmake -B build -S . -G "Ninja" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build

# Server only (also builds dependencies)
.\src\run.ps1

# Or run binaries from the build directory
.\build\chat_server.exe
.\build\chat_client.exe
```

Run the server from a working directory where `data/db.txt` is reachable (`config::DB_PATH` is `data/db.txt` relative to the process CWD). Prefer the repo root.

## Configuration

`include/config/config.h` (header-only, no `src/config`):

| Constant | Default | Meaning |
|----------|---------|---------|
| `config::PORT` | `5555` | TCP listen port |
| `config::THREAD_COUNT` | `4` | Worker threads in `ThreadPool` |
| `config::DB_PATH` | `data/db.txt` | Text database path |

## CMake targets

| Target | Role |
|--------|------|
| `utils` | Models: logger, log message, user, message, room |
| `server_lib` | Server + `src/auth/authentication.cpp` |
| `chat_server` | `src/server/main.cpp` |
| `client_lib` | Client sources |
| `chat_client` | `src/client/main.cpp` |

## Status

**Implemented:** domain models, singleton logger, text DB CRUD for users/rooms/messages, auth against that DB, Winsock listen/bind, thread pool, packet type routing, client welcome menu and packet builders.

**Not finished:** accept/read/write on client sockets, real client TCP, command parser, heartbeat, room membership and broadcast on the wire, multi-server sync, WebSocket or GUI clients.
