# Steps: distributed chat (SQLite + gossip)

This is the implementation plan. Today the repo is a **single-process TCP server** (auth, sessions, heartbeat). It is not a chatroom and it is not distributed.

**Target:** several `chat_server` processes. Each owns its own SQLite file. Clients keep the existing packet protocol. Servers replicate events over a gossip mesh.

```
Client A  --TCP:5555-->  Node 1 (data/node-1.db)
                              |  gossip TCP :5557
Client B  --TCP:5556-->  Node 2 (data/node-2.db)
```

- SQLite stores users, rooms, and history.
- Gossip delivers live messages and presence to other nodes.
- Do **not** point two processes at the same `.db` file.
- Do **not** start gossip until one server can broadcast to its own clients.

Use **eager push to all known peers** (N is small: 2–4) plus **periodic anti-entropy** so a node that was down still catches up.

---

## Current vs target

| Area | Today | After these steps |
|---|---|---|
| Process | One server, `127.0.0.1:5555` | N servers, different client + peer ports |
| Store | In-memory `Data::users` stub | Per-node SQLite (`data/node-<id>.db`) |
| `MESSAGE` / rooms | Reply string only; `RoomManager::broadcast` is TODO | Local fan-out + gossip |
| Message ids | Process-local counter in `Message` | `nodeId-timestamp-counter` |
| Inter-server | None | `GossipManager` on `PEER_PORT` |
| Login uniqueness | `hasSession` on this process only | Presence gossip + membership table |

Relevant files today:

- Config: `include/config/config.h`
- Packets: `include/utils/models/packet.h`, `src/utils/serializer.cpp`
- Server I/O: `src/server/connection_manager.cpp`, `src/server/packet_processor.cpp`
- Rooms/messages (stubs): `src/server/room_manager.cpp`, `src/server/message_manager.cpp`
- DB stub: `include/server/database_manager.h`, `src/data/data.cpp`
- Client: `src/client/client.cpp`, `packet_handler.cpp`, `console_ui.cpp`, `packet_builder.cpp`

---

## Phase 0 — Config and identity

Do this first so later code has a node id on every message.

### Done : Step 0.1 — Per-process settings

**Files:** `include/config/config.h`, `src/server/main.cpp`

Add (argv or a small config file; two processes cannot share the same values):

| Setting | Example node 1 | Example node 2 | Why |
|---|---|---|---|
| `NODE_ID` | `node1` | `node2` | Prefix on message ids, logs, gossip origin |
| `PORT` | `5555` | `5556` | Client listen (already exists; make it per-process) |
| `PEER_PORT` | `5557` | `5558` | Separate socket for gossip |
| `PEERS` | `127.0.0.1:5558` | `127.0.0.1:5557` | Other nodes’ peer ports |
| `DB_PATH` | `data/node-1.db` | `data/node-2.db` | This node’s SQLite file |

Keep existing `THREAD_COUNT`, `HEARTBEAT_INTERVAL`, `HEARTBEAT_TIMEOUT`.

`Server::dashboard()` should print node id, both ports, peer list, and db path.

### Done : Step 0.2 — Globally unique message ids

**Files:** `include/utils/models/message.h`, `src/utils/models/message.cpp`

Today `Message` uses a process-local `std::atomic` counter (`++next_message_id`). Change the id to:

```
<nodeId>-<unix_time>-<local_counter>
```

Example: `node1-1725720000-42`.

Gossip dedupes on this string. Without it, node 2 treats the same chat line as a new row forever. Do not use wall clock alone as uniqueness (clock skew).

---

## Phase 1 — Real chatroom on one server

Until this works, gossip has nothing useful to copy.

### Done : Step 1.1 — Room membership is session state

**Files:** `src/server/packet_processor.cpp`, `src/server/room_manager.cpp`, `include/server/room_manager.h`, `include/server/client_session.h`, `src/server/connection_manager.cpp`

Today `ROOM_JOIN` only sets `response.message = "joined " + packet.room`. Change it so that:

1. Parse `packet.room` into a `Room` (start with Lobby + a few named rooms).
2. Call `session.setRoom(room)`.
3. `RoomManager` keeps `roomName → list of sockets` (or usernames). Join appends this session’s socket; leave removes it.
4. Logout, heartbeat timeout, and disconnect must `leaveRoom` so the map does not hold dead sockets.

`ConnectionManager` already has `sessions` and `sendPacket`. Broadcast iterates **sockets in that room** and `sendPacket`s a `MESSAGE` packet to each (optionally skip the sender).

`RoomManager::broadcast` must talk to live sessions, not the database.

### Done : Step 1.2 — Handle `MESSAGE` for real

**File:** `src/server/packet_processor.cpp` (`case PacketType::MESSAGE`)

1. Reject if `!session.isAuthenticated()`.
2. Build a `Message` with the global id, `from = session.getUser()`, content = `packet.message`, room = `session.getRoom()`.
3. Save it (memory until Phase 2, then SQLite).
4. Call `broadcast(room, packet)` so **other clients on this process** see it.
5. Reply to the sender with `SUCCESS` (ack). Other clients need a **push** `MESSAGE` packet, not only the sender’s ack.

Do not leave the handler as `response.message = "message delivered"`.

### Done : Step 1.3 — Client UI after login

**Files:** `src/client/console_ui.cpp`, `src/client/client.cpp`, `src/client/packet_handler.cpp`, `src/client/packet_builder.cpp`

Dashboard must be able to:

- join / leave a room (`buildJoinRoom` / `buildLeaveRoom` already exist)
- send a line (`buildMessage`)
- print incoming `MESSAGE` packets (the reader thread already queues non-heartbeat packets)

`handlePacket` today is login-oriented. Incoming chat is a different path: print `sender` + `message` and do not `any_cast<User>`.

### Done : Step 1.4 — Phase 1 done when

Two `chat_client`s on **one** server, same room, typing is visible on both.

---

## Phase 2 — SQLite as this node’s store

Replace `Data::users` / maps. Each node gets its own file.

### Step Done : 2.1 — Library and CMake

**File:** `CMakeLists.txt`

On Windows, add SQLite to the server (amalgamation `sqlite3.c` / `sqlite3.h` in the repo, or vcpkg `sqlite3`).

Compile `sqlite3.c` into `server_lib`, or `find_package(SQLite3)` and `target_link_libraries(server_lib PRIVATE SQLite::SQLite3)`. Only the server needs it.

### Done : Step 2.2 — Open the DB

**Files:** `include/server/database_manager.h`, `src/server/database_manager.cpp`

Keep the singleton; change the backend. On construction:

1. `sqlite3_open(config::DB_PATH)`
2. `PRAGMA journal_mode=WAL;` — readers do not block writers as badly
3. `PRAGMA foreign_keys=ON;`
4. `PRAGMA busy_timeout=5000;` — gossip and a client write at once: wait instead of failing immediately
5. Run `CREATE TABLE IF NOT EXISTS` (schema below)

Wrap writes in `BEGIN IMMEDIATE; … COMMIT;` so a gossip insert and a login cannot interleave half-updated.

The SQLite C API is not freely concurrent on one connection. Use **one** `sqlite3*` and **one** `dbMutex` around prepare/exec.

### Done : Step 2.3 — Schema

```sql
CREATE TABLE users (
  username  TEXT PRIMARY KEY,
  email     TEXT NOT NULL,
  password  TEXT NOT NULL,
  user_type TEXT NOT NULL
);

CREATE TABLE rooms (
  name     TEXT PRIMARY KEY,
  type     TEXT NOT NULL,
  privacy  TEXT NOT NULL
);

CREATE TABLE messages (
  id         TEXT PRIMARY KEY,  -- gossip dedupe key
  room       TEXT NOT NULL,
  sender     TEXT NOT NULL,
  content    TEXT NOT NULL,
  created_at INTEGER NOT NULL,
  origin     TEXT NOT NULL      -- NODE_ID that first accepted it
);

CREATE TABLE membership (
  username TEXT NOT NULL,
  room     TEXT NOT NULL,
  node_id  TEXT NOT NULL,       -- which server holds the TCP socket
  PRIMARY KEY (username, room)
);

CREATE TABLE seen_events (
  event_id TEXT PRIMARY KEY     -- optional; messages.id can be the seen set for chat
);
```

Seed Lobby and stub users (`omri`, `spitzer`, `admin`) with `INSERT OR IGNORE`.

`membership` is **presence**, not history. When a user joins on node 1, the row is `(alice, lobby, node1)`. Gossip updates other nodes’ copies so login uniqueness can be cluster-wide.

Sockets cannot live in SQLite. Keep `roomName → sockets` in memory; persist membership for other nodes.

### Done : Step 2.4 — Rewrite `DatabaseManager` API

Keep methods you already call; implement them with SQL:

| Method | SQL |
|---|---|
| `getUser(username)` | `SELECT username, email, user_type FROM users WHERE username=?` — **no** password column, **no** password argument |
| `loginUser(username, password)` | `SELECT username, email, user_type FROM users WHERE username=? AND password=?`. If empty: `userExists` to distinguish "not found" vs "wrong password" |
| `createUser` | `INSERT INTO users`. On `SQLITE_CONSTRAINT` the username already exists — do not check-then-insert |
| `userExists(username)` | `SELECT 1 FROM users WHERE username=?` (username **only**) |
| `saveMessage` | `INSERT OR IGNORE INTO messages` — required so gossip retries are idempotent |
| `loadHistory` | `SELECT … FROM messages WHERE room=? ORDER BY created_at` |
| `setMembership` / `clearMembership` | `INSERT OR REPLACE` / `DELETE` |

Register uses `userExists(username)` (or `getUser(username)`). Do **not** call `loginUser` or `getUser(sender, message)` when `message` is `password|email`.

### Done : Step 2.5 — Wire managers

- Login / register: read/write `users`.
- `MessageManager::send`: `INSERT OR IGNORE`, then local broadcast.
- `RoomManager::joinRoom` / `leaveRoom`: update `membership` **and** the in-memory socket map.

Drop `src/data/data.cpp` as the source of truth once SQLite is live (or leave it unused).

### Step 2.6 — Phase 2 done when

Restart **one** server: users and chat history are still there. Two processes with **different** db files still do not see each other’s history — that is Phase 3.

---

## Phase 3 — Gossip mesh

### Step 3.1 — Mental model

Each node keeps:

- **Peers:** TCP connections to other servers (reconnect loop if down).
- **Rumor:** `{eventId, type, payload, originNode, hop}`.
- **Seen set:** `eventId`s already applied (`messages.id` plus presence event ids).

Rules:

1. If `eventId` is already seen → drop (ack optional).
2. Else apply locally (DB + local sockets).
3. Forward to peers **except** the one you heard it from, and never back to `origin` if you track that. For 2 nodes, “forward to the other one” is enough.
4. Cap hops (`hop < 8`) so a bug cannot flood forever.

Do **not** gossip client `HEARTBEAT` pings. Those are per-socket keepalive only.

### Step 3.2 — New packet types

**Files:** `include/utils/models/packet.h`, `src/utils/models/packet.cpp`, serializer string tables

Add types used **only** on `PEER_PORT`:

| Type | Meaning |
|---|---|
| `GOSSIP_HELLO` | First packet after connect: `NODE_ID` |
| `GOSSIP_EVENT` | One rumor (message, join, leave, login, logout, user created) |
| `GOSSIP_DIGEST` | Anti-entropy: list of recent `eventId`s |
| `GOSSIP_PULL` | “I am missing these ids, send full events” |

Suggested field reuse:

- `sender` = origin `NODE_ID`
- `receiver` = `"*"` or target node
- `room` = room name
- `message` = payload, e.g. `EVENT_TYPE|eventId|username|content|timestamp`

Keep client types unchanged. `PacketProcessor` on the **client** port must ignore `GOSSIP_*`. Peers must not go through user `LOGIN`.

### Step 3.3 — `GossipManager` (new class)

**New files:** `include/server/gossip_manager.h`, `src/server/gossip_manager.cpp`

Add the `.cpp` to `server_lib` in `CMakeLists.txt`.

Responsibilities:

1. **Listen** on `PEER_PORT` (copy Winsock listen/accept from `ConnectionManager`; do **not** mix peer sockets into client `sessions`).
2. **Dial** each entry in `PEERS` on startup; retry every few seconds if down.
3. **HELLO** both ways; store `socket → nodeId`. Reject duplicate `NODE_ID` (connected to yourself).
4. `rumor(event)` — if not seen: apply, persist seen, send `GOSSIP_EVENT` on all peer sockets.
5. **Anti-entropy thread** (every 2–5 s): send `GOSSIP_DIGEST` of last N message ids (and membership version). Peer replies with `GOSSIP_PULL` for missing ids; you send those full `GOSSIP_EVENT`s.

Peer send uses the same length-prefix framing as `Serializer` / `sendPacket`. Do not invent a second protocol.

`Server` owns `GossipManager` the same way it owns `Heartbeat` and `ConnectionManager`.

### Step 3.4 — Single apply path

One function used by **both** client `MESSAGE` and incoming gossip:

```
applyChatMessage(event, fromGossip):
  INSERT OR IGNORE into messages
  if (sqlite reported a new row)        -- changes() == 1
      send MESSAGE to local sockets in that room
  if (!fromGossip)
      gossip.rumor(event)
```

- If you rumor before `INSERT` and the insert fails, the cluster diverges.
- Prefer: **only gossip if this node is the first to accept it** (`fromGossip == false` and insert succeeded).
- Incoming gossip: apply, do not rumor again if you use full-mesh push.
- If a third node exists and you did not push to all, forward to peers except incoming.
- For two nodes, never re-gossip.

Same pattern for presence:

```
applyLogin(username, nodeId, fromGossip):
  if another node already has this username online → reject (only on originating node)
  upsert membership / in-memory "online"
  if (!fromGossip) rumor LOGIN
```

Presence event ids example: `node1-login-omri-1725…` (login is not a `messages` row).

### Step 3.5 — Cluster login

`hasSession` today only looks at this process. After gossip, check SQLite `membership` or an `online_users(username PRIMARY KEY, node_id)` table.

On logout or heartbeat timeout, delete the row and rumor `LOGOUT`. If you skip this, node 2 keeps “user already logged in” forever after node 1 drops the socket.

### Step 3.6 — Hook from `PacketProcessor`

| Client packet | After local success |
|---|---|
| `REGISTER` | insert user, rumor `USER_CREATED` |
| `LOGIN` | rumor presence |
| `LOGOUT` | rumor logout |
| `ROOM_JOIN` / `ROOM_LEAVE` | rumor membership; local socket map only on this node |
| `MESSAGE` | `applyChatMessage(..., fromGossip=false)` |

Handle incoming `GOSSIP_EVENT` in the gossip accept/read loop, **not** in `ConnectionManager::handleClient`.

Register on node 1 must rumor `USER_CREATED` so node 2’s `users` table matches. Otherwise login on the other server fails with not found.

`USER_CREATED` will carry a password (or hash). For a lab that is acceptable; do not log the payload. Hash later.

### Step 3.7 — What SQLite vs gossip each own

| Data | SQLite | Gossip |
|---|---|---|
| User accounts | Yes (each node) | `GOSSIP_EVENT` type `USER_CREATED` |
| Message history | Yes (`INSERT OR IGNORE`) | Push + digest repair |
| Who is in which room **on this TCP socket** | Membership table is a replica | Yes |
| Live bytes to a client | Never | Peer applies then `sendPacket` locally |

### Step 3.8 — Phase 3 done when

See Phase 4.

---

## Phase 4 — Run and prove it

1. Terminal A: `chat_server` `NODE_ID=node1` client `5555` peer `5557` peers=`127.0.0.1:5558` db=`data/node-1.db`
2. Terminal B: `node2`, client `5556`, peer `5558`, peers=`127.0.0.1:5557`, db=`data/node-2.db`
3. Logs show `GOSSIP_HELLO` both ways.
4. Client A → `:5555`, client B → `:5556`, both join `Lobby`, A sends a line → B’s console shows it.
5. Kill node 2, A sends more lines (saved on node 1). Restart node 2 → anti-entropy digest → node 2 DB contains the missed rows (history). A live client on node 2 only sees **new** messages after it reconnects; that is expected.
6. Login `omri` on node 1, try `omri` on node 2 → rejected.

---

## Phase 5 — Details that will break you if skipped

- **Seen set vs `INSERT OR IGNORE`.** Primary key on `messages.id` is the durable seen set. Presence events need their own ids.
- **Clock skew.** Node id + counter is enough for uniqueness. `created_at` is for UI ordering only.
- **Thread safety.** Client threads, heartbeat, and gossip all touch sessions and SQLite. One mutex for `sessions`; one `dbMutex` around SQLite.
- **Do not share `node-1.db` on a network drive.** Two nodes, two files. Gossip copies rows. SQLite on a network share is not a cluster store.
- **Partial mesh.** For node 3, eager-push to **all** peers (full mesh). Random “pick 1 peer” gossip needs more hops; save it until 2-node push works.
- **Client discovery.** Manual host/port is enough. A directory service is optional.
- **Do not mix ports.** Client packets on `PORT`; gossip on `PEER_PORT`.

---

## Implementation order (checklist)

Work top to bottom. Do not skip.

- [ ] **0.1** Config: `NODE_ID`, two ports, peer list, per-node `DB_PATH`
- [ ] **0.2** Global message ids (`nodeId-timestamp-counter`)
- [ ] **1.1** Join/leave updates session + `RoomManager` socket map; cleanup on logout/disconnect
- [ ] **1.2** `MESSAGE` saves, broadcasts to local sockets, acks sender
- [ ] **1.3** Client dashboard: join/leave/send/print incoming chat
- [ ] **1.4** Prove: two clients, one server, same room
- [ ] **2.1** SQLite in CMake / `server_lib`
- [ ] **2.2** Open DB with WAL, foreign keys, busy timeout, mutex
- [ ] **2.3** Schema + seed Lobby and stub users
- [ ] **2.4** `DatabaseManager` on SQL; register checks username only
- [ ] **2.5** Wire `MessageManager` / `RoomManager` / login
- [ ] **2.6** Prove: restart one server, data remains
- [ ] **3.2** Packet types: `GOSSIP_HELLO`, `GOSSIP_EVENT`, `GOSSIP_DIGEST`, `GOSSIP_PULL`
- [ ] **3.3** `GossipManager` listen / dial / HELLO
- [ ] **3.4** `applyChatMessage` + rumor messages only
- [ ] **3.4** Prove: client A on `:5555`, client B on `:5556`, messages both ways
- [ ] **3.3** Digest / pull after restart (missed history)
- [ ] **3.5–3.6** Gossip register + login/logout presence
- [ ] **3.6** Gossip join/leave so membership tables match
- [ ] **4** Full prove list (hello, chat, catch-up, duplicate login rejected)

If you skip Phase 1, you will debug gossip while the real bug is “broadcast never sent to local sockets.” If you skip `INSERT OR IGNORE`, every digest repair duplicates chat lines.
