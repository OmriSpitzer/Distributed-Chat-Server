# Improvements & things to do

Backlog for the distributed chat system. Priority is approximate; items marked **(goal)** are explicit design targets.

**Docs:** [README.md](README.md) · [architecture.md](architecture.md) · [database.md](database.md) · [tests/TESTS.md](tests/TESTS.md)

---

# ------------------------------ VERSION 2.0 ------------------------------

## 1. Persistence — one mapper, not the whole domain **(goal)**

`DatabaseManager` should be the **only place that turns SQL rows into domain objects**. It does **not** own the domain: `PacketProcessor` and other app code may still build `User` / `Message` / `Room` from validated input (login fields, gossip payloads, etc.).

- [x] Keep all row → object mapping inside `DatabaseManager` (`User`, `Room`, `Message`, presence/membership helpers). No raw column parsing in gossip apply, processors, or managers.
- [x] Add missing load/save APIs that return objects: `getRoom` / `listRooms` / `createRoom` / `deleteRoom` (or equivalent).
- [x] Room `id` is SQLite `INTEGER PRIMARY KEY AUTOINCREMENT` — assigned on insert (`sqlite3_last_insert_rowid`), not by the `Room` constructor. Constructor takes an explicit `int id` (from DB or seed). Messages/membership FKs use `room_id`.
- [x] Application code may construct objects from validated input; persistence only maps and stores them.
- [x] Keep password hashing inside create/login; never put hash/plaintext on objects meant for UI/wire.

---



## 2. Schema — constrained enums in the DB **(goal)**

C++ already has `User::UserType`, `Room::RoomType`, `Room::Privacy`, `Packet::PacketType`. SQLite still stores free `TEXT` (`user_type`, `type`, `privacy`).

Prefer `TEXT` **+** `CHECK` against the known set (readable, stays stable if C++ enum order changes). `INTEGER` mapped to enums is also fine if you prefer it — pick one and stick to it.

- [x] Constrain enum-like columns (`CHECK` on TEXT, or INTEGER + mapped helpers).
- [x] Migrate `init.sql` seed data and all queries to the chosen representation.
- [x] Private-room `allow_list` table + join / invite / gossip ACL paths.

---



## 3. TCP layer — clearer transport **(goal)**

Framing works (`socket_io` + `Serializer`), but sockets are still raw `int` / cast `SOCKET`, split across `ConnectionManager`, `Network`, and gossip.

- [x] Introduce a small, explicit TCP API (listen / accept / connect / read-frame / write-frame / close) with one socket type end-to-end — no `int` ↔ `SOCKET` casts at call sites.
- [x] Keep packet framing (`[u32 BE size][payload]`) only in that layer; higher layers deal only in `Packet`.
- [x] Align client `Network` and server `ConnectionManager` on the same helpers.

---



## 4. Client product gaps

- [x] **Update profile** — dashboard / console (`UPDATE_USER` via `PacketBuilder` / `ConsoleUI::showUpdateProfile`).
- [x] **Room directory** — console should list available rooms before join (`ConsoleUI::showJoinRoom`).
- [x] **Validate room exists** before sending `ROOM_JOIN` (or surface clear `404` from server).
- [x] **Message history** — server has `loadHistory`; client requests via `LOAD_MESSAGE_HISTORY`.
- [x] **Pushed messages in UI** — console and Qt dashboard drain queued room pushes.

---



## 5. Rooms & authorization

- [x] Wire protocol for `createRoom` (`ROOM_CREATE` + `ROOM_LIST` push) and `deleteRoom` (`ROOM_DELETE`, ADMIN).
- [x] Enforce **PRIVATE** vs **PUBLIC** via `allow_list` on join / invite.
- [x] Guests may join public rooms (in-memory); private rooms require authenticated allow-list membership.
- [x] Enforce **ADMIN** privilege checks (seed has `ADMIN`):
  - Invite or kick a user from **any** room (not only the creator).
  - Create or delete rooms, including **PRIVATE** — never Lobby or General.
  - Join **PRIVATE** rooms (bypass `allow_list`).
- [x] Unique **email** constraint coverage and clear client errors (username uniqueness is stronger today).

---



## 6. Server runtime & architecture

- [x] `ThreadPool`: kept constructed with `Server` and shut down on stop; session I/O stays on dedicated threads (pool is not the accept path).
- [x] Singletons (`DatabaseManager`, `RoomManager`) are acceptable for this small node. Prefer DI (owned by `Server`) only if tests need it — not a must.
- [x] Cap / rotate gossip event log: verify ops story when `MAX_EVENT_LOG` drops old ids.

---



## 7. Security & correctness

- [x] **USER_CREATED gossip** rumors Argon2id hash only (not plaintext) for peer register.
- [x] Replace placeholder Argon2 hashes in `init.sql` seed users with real hashes (`admin`/`user`).
- [x] Reject oversized / malformed frames without tearing down the process (robustness).

---



## 8. Testing (from `tests/TESTS.md`)

Highest-value gaps:

- [x] E2E: two clients, same room, MESSAGE push received (live `Server` + `Client` in `tests/func/`).
- [x] E2E: join / leave Lobby rules, unknown room `404`, logout/login, double-login rejected.
- [x] Cluster unit coverage: LOGIN / USER_CREATED / ROOM_JOIN / MESSAGE / ROOM_CREATED+ACL / anti-entropy DIGEST-PULL (`gossip_manager_test`).
- [x] Heartbeat: live pong keeps session; silent client timed out and presence cleared.

---



## 9. Docs & cleanup

- [x] Layered architecture documented in [architecture.md](architecture.md); schema in [database.md](database.md); README points at both.
- [x] PowerShell helpers under `scripts/` (`build`, `run_server`, `run_client`, `run`, `run_cluster`).
- [x] README “not in this tree” list synced with shipped features (profile, rooms, TCP helpers, DB enums, allow_list).
- [x] Decide product scope for later: WebSocket / shared remote DB.

---



## 10. Visual GUI — server & client

Qt Widgets dashboard; keep console entry points for tests/scripts. Presentation-only widgets; no Qt inside `server_lib` domain/transport beyond a `Server*` / singleton read from the GUI thread.

### Server GUI

- [x] Qt build wire-up (`find_package(Qt6 Widgets)`, AUTOMOC, `chat_server` sources).
- [x] `MainPage` / `MainWindow` shell + abstract `Panel` base.
- [x] **Ports** panel — `config::` node / ports / peers / db.
- [x] **Users** panel — timer refresh from `Server::connections().getSessions()`.
- [x] **Rooms** panel — timer refresh from `RoomManager::listRooms()`.
- [x] **Log** panel — timer refresh from `Logger`.
- [x] Console ↔ GUI choice in `main` (real flag/prompt; not `if (true)`).



### Client GUI

- [x] Qt client dashboard (`DashboardPage` / `MainPage`) beside console `ConsoleUI` (`--test`).
- [x] Screens: home/login/register, dashboard, join/create/delete room, messaging, profile, invite/kick, history.
- [x] Reuse `Client` / `PacketBuilder` / `PacketHandler` / `Network` — GUI is presentation only.
- [x] Show **pushed messages** live (pairs with §4 console push presentation).
- [x] GUI-thread rules: no widget updates from reader/network threads without queued signals.

---



# ------------------------------ VERSION 3.0 ------------------------------

## 1. Ambassedor Design Patern

Introduce a client-side Ambassador as the only path from `Client` to the remote server, so UI/application code stays protocol-agnostic and new transports (WebSocket, REST, …) plug in as adapters.

- [ ] Define an Ambassador interface with domain operations (`login`, `signUp`, `logout`, `joinRoom`, `leaveRoom`, `createRoom`, `deleteRoom`, `invite`, `kick`, `sendMessage`, `updateProfile`, `loadHistory`, connect/disconnect, health).
- [ ] Return typed domain results / clear error strings — never expose raw `Packet`, sockets, or protocol status codes to `Client` / UI.
- [ ] Keep pushes (`ROOM_LIST`, live `MESSAGE`) behind the same façade (callbacks or a small inbox API), not a second path into `Network`.

- [ ] Implement a TCP/`Packet` adapter that owns `PacketBuilder`, `Network`, `waitFor`, and `PacketHandler`.
- [ ] Align `PacketHandler` with **all** RPC success paths (not only LOGIN / REGISTER / UPDATE_USER): validate `responseCode` **and** payload; refuse `200` with empty/malformed bodies.
- [ ] Move reply parsing out of `Client` action methods; `Client` only applies Ambassador results to `ClientState`.
- [ ] Treat `responseCode == 0` pushes as unsolicited events, not RPC success.

- [ ] Wire `Client` to call only the Ambassador (no direct `handler.handlePacket` / duplicated `responseCode` checks).
- [ ] Preserve console + Qt behavior (same public `Client` methods; internals swap to Ambassador).
- [ ] Unit-test Ambassador with a fake adapter; keep `PacketHandler` tests protocol-local (TCP adapter).

- [ ] Client health check against the connected server (pairs with § Architecture “Client health checks…”).
- [ ] Timeouts / reconnect policy on the Ambassador (not in UI code).
- [ ] Optional: retries / circuit-break only for idempotent ops; document which calls may retry.

- [ ] Document adapter contract so WebSocket / REST implement the same Ambassador operations.
- [ ] Do **not** reuse `PacketHandler` for non-`Packet` protocols — each adapter has its own codec mapping to the same domain types.
- [ ] Add a second adapter only when a real second transport ships; until then keep one TCP adapter behind the interface.



## 2. Implementing design patterns

- [ ] Sidecar Design Patter - Dynamic logic service changers in servers



## 3. Architecture

- [ ] Clear cluster presence on node boot (`online_users` should not survive a crash as “still online”).
- [ ] Client health checks the server he is connected to
- [ ] Live peer sockets on Ports (optional `GossipManager` ~~snapshot getters).~~



## 4. Gossip payload

- [ ] Inhance the payload to be object oriented and scalable (not defined by 5 fields)
- [ ] RESTfulness on API's