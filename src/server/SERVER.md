# Server (`src/server`)

TCP chat server: Winsock listen socket, per-connection session objects, a fixed worker pool, and a packet router that talks to auth and in-memory/DB managers.

Headers: `include/server/`. Linked as `server_lib` plus executable `chat_server` from `main.cpp`. Uses `ws2_32` and `utils`.

## Files

### `main.cpp`

Process entry. Constructs `Server`, calls `start()` and `dashboard()`, then spins on `isAlive()` until that flag is false, then `stop()`. There is no accept loop or stdin command to shut down, so the process stays up after a successful start.

### `server.cpp` / `include/server/server.h`

Orchestrator. Owns `ThreadPool` (`config::THREAD_COUNT`), `ConnectionManager`, and `PacketProcessor`.

| Method | Behavior |
|--------|----------|
| `start()` | `WSAStartup(2.2)`, then `connectionManager.startListening(config::PORT)`. Sets `running` on success. |
| `stop()` | Stops listening, shuts down the pool, `WSACleanup()`. |
| `isAlive()` | Returns `running`. |
| `dashboard()` | Prints port, thread count, DB path, and listen state. |

Does not yet enqueue accept or I/O work on the thread pool.

### `connection_manager.cpp` / `include/server/connection_manager.h`

Owns the listening socket and a (unused so far) map of `ClientSession` by socket fd.

- `startListening(port)` — `socket` / `SO_REUSEADDR` / bind `INADDR_ANY` / `listen(SOMAXCONN)`.
- `stopListening()` — `closesocket` on the listen fd.
- `getListeningSocket()`, `isListening()`.

Not copyable. Destructor stops listening. No `accept` yet.

### `client_session.cpp` / `include/server/client_session.h`

State for one connection: socket, `User`, `Room`, `authenticated` flag (starts false). Getters/setters for user and auth. Used by `PacketProcessor` on login/logout.

### `packet_processor.cpp` / `include/server/packet_processor.h`

`processPacket(packet, session)` builds a reply (`sender = "server"`, same type/room, current timestamp) and switches on `Packet::PacketType`:

| Type | Action |
|------|--------|
| `LOGIN` | `Authentication::login(sender, message)`; on success sets session user and authenticated. |
| `LOGOUT` | `Authentication::logout`; clears authenticated. |
| `MESSAGE` | `"message delivered"` if authenticated, else `"not authenticated"`. Does not persist. |
| `ROOM_JOIN` / `ROOM_LEAVE` | Status string only; does not call `RoomManager`. |

Holds `Authentication`, `RoomManager`, `MessageManager`, and `UserManager` members; only authentication is used in the switch.

### `thread_pool.cpp` / `include/server/thread_pool.h`

Fixed pool: constructor starts `numThreads` workers. `task(fn)` queues work. `shutdown()` sets `stopping`, wakes waiters, joins. `workerLoop` waits on a condition variable. Server creates the pool but never calls `task()`.

### `database_manager.cpp` / `include/server/database_manager.h`

Singleton text DB at `config::DB_PATH`. Ensures parent dirs and a file with `[USERS]`, `[ROOMS]`, `[MESSAGES]`.

| API | Persistence |
|-----|-------------|
| `createUser` / `userExists` / `findUser` / `getPassword` | `username\|email\|userType\|password` |
| `createRoom` | `id\|name\|type\|privacy` |
| `saveMessage` / `loadMessages` | `id\|roomId\|from\|to\|content\|timestamp` |

`findUser` throws if missing. `loadMessages` keeps reconstructed `User` objects in `userPool` so `Message` references stay valid. Not copyable.

### `user_manager.cpp` / `include/server/user_manager.h`

In-memory map of online users. `addUser` inserts and calls `DatabaseManager::createUser` (no password). `removeUser` is memory-only. `findUser` checks memory then the DB. `setStatus` prints to stdout.

### `room_manager.cpp` / `include/server/room_manager.h`

`createRoom` writes through `DatabaseManager`. `deleteRoom`, `joinRoom`, `leaveRoom`, and `broadcast` only print to stdout.

### `message_manager.cpp` / `include/server/message_manager.h`

`sendPrivate` / `saveMessage` persist via `DatabaseManager`. `loadHistory` loads and prints a count.

### `heartbeat.cpp` / `include/server/heartbeat.h`

Stub: `start()` prints `"Heartbeat started"`. Not called from `Server`.

## Related

- Auth used by the processor: [../auth/AUTH.md](../auth/AUTH.md)
- Packet and domain types: [../utils/models/MODELS.md](../utils/models/MODELS.md)
- DB file format: [../data/DATA.md](../data/DATA.md)
- Listen port and pool size: `include/config/config.h`
