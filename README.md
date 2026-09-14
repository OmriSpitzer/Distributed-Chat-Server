# Distributed Chat Server

[![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)](https://isocpp.org/)[![CMake](https://img.shields.io/badge/CMake-3.16%2B-064F8C?logo=cmake&logoColor=white)](https://cmake.org/)[![Platform](https://img.shields.io/badge/Windows-Winsock-0078D6?logo=windows&logoColor=white)](#quick-start)[![Tests](https://img.shields.io/badge/tests-Catch2%20v3.8-FF6B2B)](https://github.com/catchorg/Catch2)[![Version](https://img.shields.io/badge/version-1.0.0-informational)]()

C++17 chat cluster for Windows. Clients talk to a TCP server over framed binary packets; servers gossip with each other so presence, membership, and messages stay in sync across nodes.

```
  chat_client ──TCP :PORT──►  chat_server ──gossip :PEER_PORT──►  peer servers
                                   │
                                   ▼
                              SQLite (per node)
```

A console client (`chat_client`) and a Winsock server (`chat_server`) ship in this repo. Passwords are hashed with Argon2id. Each node keeps its own SQLite file and replicates events over gossip rather than sharing a single store.

Class-level diagrams live in [architecture.md](architecture.md).

## Features

- **Auth** — register and login with Argon2id; one live session per user across the cluster
- **Rooms** — join / leave rooms, default public Lobby, in-room broadcast
- **Messages** — persist to SQLite, fan out to local sockets, rumor to peer nodes
- **Heartbeat** — server pings; client auto-replies `pong`; stale sockets are closed
- **Gossip** — `HELLO`, `EVENT`, `DIGEST`, and `PULL` with periodic anti-entropy
- **Console UI** — login, register, join room, send message, logout

## Architecture

```
                    +------------------+
                    |  Console client  |
                    +--------+---------+
                             |  TCP :PORT
                             |  Packet (Serializer + socket_io)
          +------------------+------------------+
          |                                     |
   +------+-------+      gossip :PEER_PORT     +--------+------+
   | Chat Server 1|<-------------------------->| Chat Server 2 |
   +------+-------+                            +--------+------+
          |  SQLite                                  |  SQLite
          +------------------+------------------+----+
                             |
              users · rooms · messages · membership · online_users
```

**Client path.** `ConsoleUI` builds a `Packet` via `PacketBuilder`. `Network` writes it on the socket (background reader thread pongs heartbeats and logs pushed messages). The server `ConnectionManager` accepts the connection, owns a `ClientSession`, and hands the packet to `PacketProcessor`.

**Server path.** `PacketProcessor` authenticates against `DatabaseManager`, updates `RoomManager`, and rumors a `GOSSIP_EVENT` through `GossipManager`. Gossip applies the event locally (persist + optional room broadcast) and forwards it to peers. Anti-entropy digests catch nodes that missed a rumor.

**Wire format.** Length-prefixed binary frames, max 1 MiB payload:

```
[u32 BE size][u8 type][u64 BE timestamp][u32 BE responseCode]
  then sender, receiver, room, message as [u32 BE length][bytes]
```

Gossip event bodies use five length-prefixed fields (`type`, `eventId`, `username`, `content`, `field5`) so payloads may contain `|`.

## Packet types

| Type | Role |
|------|------|
| `LOGIN` / `REGISTER` / `LOGOUT` | Session lifecycle; login/register success puts room directory in `room` |
| `ROOM_JOIN` / `ROOM_LEAVE` | Membership (`LEAVE` returns to Lobby) |
| `ROOM_CREATE` | Create room (`room` = name, `message` = optional `type\|privacy`) |
| `ROOM_LIST` | Server push of full directory (`responseCode == 0`, body in `message`) |
| `MESSAGE` | Chat send (response) and room push (`responseCode == 0`) |
| `HEARTBEAT` | Server `ping` / client `pong` |
| `UPDATE_USER` | Profile update |
| `GOSSIP_HELLO` / `GOSSIP_EVENT` / `GOSSIP_DIGEST` / `GOSSIP_PULL` | Peer port only |

Responses use `200` success, `400` error, `404` not found, `500` internal.

## Quick start

Requires **CMake 3.16+**, a **C++17** compiler, and **Ninja** if you use the PowerShell helpers. The server and client link **Winsock** (`ws2_32`). Catch2 v3.8.1 and Argon2 are fetched at configure time.

```powershell
cmake -B build -S . -G "Ninja" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build

# Server (listens on 5555, gossip on 5557)
.\src\server.ps1
# or
.\build\chat_server.exe

# Client (connects to 127.0.0.1:5555)
.\src\client.ps1
# or
.\build\chat_client.exe
```

SQLite files are created under `data/` relative to the process working directory (gitignored). Prefer running from the repo root.

### Two-node cluster

```powershell
.\build\chat_server.exe --node-id node1 --port 5555 --peer-port 5557 --peers 127.0.0.1:5558 --db data/node-1.db
.\build\chat_server.exe --node-id node2 --port 5556 --peer-port 5558 --peers 127.0.0.1:5557 --db data/node-2.db

.\build\chat_client.exe --host 127.0.0.1 --port 5555
.\build\chat_client.exe --host 127.0.0.1 --port 5556
```

`--peers` is a comma-separated list of **gossip** addresses (`host:port`), not client ports.

## Configuration

CLI flags map onto `include/config/config.h`:

| Flag | Default | Meaning |
|------|---------|---------|
| `--node-id ID` | `node1` | Cluster identity for this process |
| `--host HOST` | `127.0.0.1` | Client connect host |
| `--port N` | `5555` | Client TCP listen / connect port |
| `--peer-port N` | `5557` | Gossip listen port |
| `--peers H:P,H:P` | *(empty)* | Other nodes' gossip addresses |
| `--db PATH` | `data/node-1.db` | SQLite path |
| `--help` | | Print usage |

Heartbeat interval is 5 s; timeout is 10 s. Worker thread count is 4.

## Repository layout

```
Distributed-Chat-Server/
├── CMakeLists.txt          Libraries, executables, Catch2 tests
├── architecture.md         Class and sequence diagrams
├── include/                Public headers
│   ├── auth/               Argon2id wrapper
│   ├── client/             Network, UI, packets, client state
│   ├── config/             Ports, peers, DB path, CLI parser
│   ├── server/             Sessions, rooms, gossip, heartbeat
│   └── utils/              Packet, serializer, socket_io, models
├── src/
│   ├── server.ps1          Configure, build, run chat_server
│   ├── client.ps1          Configure, build, run chat_client
│   ├── auth/
│   ├── client/
│   ├── database/           init.sql + queries (baked into sql_schemas.h)
│   ├── server/
│   └── utils/
└── tests/                  auth · client · server · utils
```

Headers live in `include/`; implementations live in `src/`. CMake adds `include/` as a public include path and embeds SQL files from `DB_SCHEMAS` into a generated `sql_schemas.h` at configure time.

## CMake targets

| Target | Role |
|--------|------|
| `utils` | Models, serializer, socket I/O, gossip payload |
| `auth` | Argon2id hash / verify |
| `server_lib` / `chat_server` | Server library and executable |
| `client_lib` / `chat_client` | Client library and executable |
| `sqlite3` | Bundled SQLite amalgamation |
| `*_test` | Catch2 binaries (discovered by CTest) |

```powershell
ctest --test-dir build --output-on-failure
```

## Status

**Working today:** TCP accept and framed I/O, console login/register/logout, room join/leave, message persist and broadcast, heartbeat, gossip rumor + anti-entropy, cluster-wide single login, Catch2 coverage for utils, auth, client, and server.

**Not in this tree:** profile updates (menu stub), room directory in the console UI, WebSocket or GUI clients, a shared remote database. `ThreadPool` is constructed but session I/O currently runs on dedicated threads.
