# Distributed Chat Server

 [![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)](https://isocpp.org/) [![CMake](https://img.shields.io/badge/CMake-3.16%2B-064F8C?logo=cmake&logoColor=white)](https://cmake.org/) [![Platform](https://img.shields.io/badge/Windows-Winsock-0078D6?logo=windows&logoColor=white)](#quick-start) [![Tests](https://img.shields.io/badge/tests-Catch2%20v3.8-FF6B2B)](https://github.com/catchorg/Catch2) [![Version](https://img.shields.io/badge/version-1.0.0-informational)]()

C++17 chat cluster for Windows. Clients talk to a TCP server over framed binary packets; servers gossip with each other so presence, membership, and messages stay in sync across nodes.

```
  chat_client ──TCP :PORT──►  chat_server ──gossip :PEER_PORT──►  peer servers
                                   │
                                   ▼
                              SQLite (per node)
```

Qt dashboards ship on `chat_client` and `chat_server` (default). Pass `--test` for the console UIs used by scripts and Catch2. Passwords are hashed with Argon2id. Each node keeps its own SQLite file and replicates events over gossip rather than sharing a single store.

**Docs to read next**

| Doc | What it covers |
|-----|----------------|
| [architecture.md](architecture.md) | Layered classes, Mermaid diagrams, client/server call paths, wire sequence |
| [database.md](database.md) | Per-node SQLite schema, ER model, `allow_list`, query catalog, write paths |
| [tests/TESTS.md](tests/TESTS.md) | Catch2 catalog and remaining gaps |
| [FUTURE_WORK.md](FUTURE_WORK.md) | Backlog and open work |

## Features

- **Auth** — register, login, logout, and profile update with Argon2id; one live session per user across the cluster
- **Rooms** — public / private rooms, join / leave (back to Lobby), create, invite to private rooms (`allow_list`)
- **Messages** — persist for registered users, live broadcast (including guests), rumor to peer nodes, history load
- **Heartbeat** — server pings; client auto-replies `pong`; stale sockets are closed
- **Gossip** — `HELLO`, `EVENT`, `DIGEST`, and `PULL` with periodic anti-entropy
- **Qt GUI** — client chat dashboard and server Ports / Users / Rooms / Log panels (console via `--test`)

## Architecture

```
                    +------------------+
                    | Qt / console     |
                    | chat_client      |
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
              users · rooms · messages · membership · online_users · allow_list
```

**Client path.** The Qt dashboard (or `ConsoleUI`) calls `Client` action methods. `PacketBuilder` builds the `Packet`; `Network` writes it (background reader pongs heartbeats, applies `ROOM_LIST`, and queues `MESSAGE` pushes for the UI). The server `ConnectionManager` accepts the connection, owns a `ClientSession`, and hands the packet to `PacketProcessor`.

**Server path.** `PacketProcessor` authenticates against `DatabaseManager`, updates `RoomManager`, and rumors a `GOSSIP_EVENT` through `GossipManager`. Gossip applies the event locally (persist + optional room broadcast) and forwards it to peers. Anti-entropy digests catch nodes that missed a rumor.

For class diagrams and sequences, see **[architecture.md](architecture.md)**. For the SQLite layout and gossip-related persistence, see **[database.md](database.md)**.

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
| `ROOM_CREATE` | Create room (`room` = name, `message` = optional privacy) |
| `ROOM_INVITE` | Allow-list a user for a private room |
| `ROOM_LIST` | Server push of full directory (`responseCode == 0`, body in `message`) |
| `MESSAGE` | Chat send (response) and room push (`responseCode == 0`) |
| `LOAD_MESSAGE_HISTORY` | History for the current room |
| `HEARTBEAT` | Server `ping` / client `pong` |
| `UPDATE_USER` | Profile update |
| `GOSSIP_HELLO` / `GOSSIP_EVENT` / `GOSSIP_DIGEST` / `GOSSIP_PULL` | Peer port only |

Responses use `200` success, `400` error, `404` not found, `500` internal.

## Quick start

Requires **CMake 3.16+**, a **C++17** compiler, **Qt 6 Widgets**, and **Ninja** if you use the PowerShell helpers. The server and client link **Winsock** (`ws2_32`). Catch2 v3.8.1 and Argon2 are fetched at configure time.

```powershell
# Build both binaries
.\scripts\build.ps1

# Single server / client (GUI)
.\scripts\run_server.ps1
.\scripts\run_client.ps1

# Console UIs
.\scripts\run_server.ps1 --test
.\scripts\run_client.ps1 --test

# Or cmake directly
cmake -B build -S . -G "Ninja" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
.\build\chat_server.exe
.\build\chat_client.exe
```

SQLite files are created under `data/` relative to the process working directory (gitignored). Prefer running from the repo root.

### Two-node cluster

Fastest path — builds, opens **2 servers** and **2 clients** (one client per node) in separate windows:

```powershell
.\scripts\run_cluster.ps1
# console UIs:
.\scripts\run_cluster.ps1 -Test
```

Manual equivalent:

```powershell
.\scripts\run_server.ps1 --node-id node1 --port 5555 --peer-port 5557 --peers 127.0.0.1:5558 --db data/node-1.db
.\scripts\run_server.ps1 --node-id node2 --port 5556 --peer-port 5558 --peers 127.0.0.1:5557 --db data/node-2.db

.\scripts\run_client.ps1 --host 127.0.0.1 --port 5555
.\scripts\run_client.ps1 --host 127.0.0.1 --port 5556
```

`--peers` is a comma-separated list of **gossip** addresses (`host:port`), not client ports.

| Script | Role |
|--------|------|
| `scripts/build.ps1` | Configure (if needed) and build `chat_server` + `chat_client` |
| `scripts/run_server.ps1` | Run one server in the current terminal |
| `scripts/run_client.ps1` | Run one client in the current terminal |
| `scripts/run.ps1` | Start one server in a new window (used by the cluster script) |
| `scripts/run_cluster.ps1` | Build + 2-node cluster + 2 clients |

Legacy helpers `src/server.ps1` and `src/client.ps1` still build-and-run a single binary; prefer `scripts/`.

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
| `--test` | off | Console UI instead of Qt |
| `--help` | | Print usage |

Heartbeat interval is 5 s; timeout is 10 s. Worker thread count is 4 (`ThreadPool` is constructed for shutdown compatibility; session I/O uses dedicated threads).

## Repository layout

```
Distributed-Chat-Server/
├── CMakeLists.txt          Libraries, executables, Catch2 tests
├── architecture.md         Class and sequence diagrams
├── database.md             SQLite schema, queries, write paths
├── STEPS.md                Backlog
├── scripts/                build / run / cluster PowerShell helpers
├── include/                Public headers
│   ├── auth/               Argon2id wrapper
│   ├── client/             Network, packets, state, Qt dashboard
│   ├── config/             Ports, peers, DB path, CLI parser
│   ├── server/             Sessions, rooms, gossip, heartbeat, Qt panels
│   └── utils/              Packet, serializer, socket_io, models
├── src/
│   ├── auth/
│   ├── client/
│   ├── database/           init.sql + queries (baked into sql_schemas.h)
│   ├── server/
│   └── utils/
└── tests/                  auth · client · server · utils (+ TESTS.md)
```

Headers live in `include/`; implementations live in `src/`. CMake adds `include/` as a public include path and embeds SQL files from `DB_SCHEMAS` into a generated `sql_schemas.h` at configure time.

## CMake targets

| Target | Role |
|--------|------|
| `utils` | Models, serializer, socket I/O, gossip payload |
| `auth` | Argon2id hash / verify |
| `server_lib` / `chat_server` | Server library and Qt/console executable |
| `client_lib` / `chat_client` | Client library and Qt/console executable |
| `sqlite3` | Bundled SQLite amalgamation |
| `*_test` | Catch2 binaries (discovered by CTest) |

```powershell
ctest --test-dir build --output-on-failure
```

See [tests/TESTS.md](tests/TESTS.md) for the case catalog (**24** executables, **339** `TEST_CASE`s).

## Status

**Working today:** framed TCP, Qt and console clients/servers (`--test` for console), register / login / logout / profile, public and private rooms (join, leave, create, invite, kick, delete), ADMIN gates (invite/kick any room, delete except Lobby/General, join PRIVATE), live chat plus history for registered users, heartbeat, gossip rumor + anti-entropy, cluster-wide single login, PowerShell cluster helpers, Catch2 coverage for utils, auth, client, and server.

**Still open:** WebSocket or a shared remote database, clear stale `online_users` on node boot, live gossip sockets on the server Ports panel, event-driven GUI refresh (panels poll on a timer). See [FUTURE_WORK.md](FUTURE_WORK.md).
