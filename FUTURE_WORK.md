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
- [ ] Clear cluster presence on node boot (`online_users` should not survive a crash as “still online”).
- [x] Cap / rotate gossip event log: verify ops story when `MAX_EVENT_LOG` drops old ids.

---



## 7. Security & correctness

- [ ] **USER_CREATED gossip** currently rumors password material for peer register — replace with hash-only (or challenge) replication.
- [ ] Replace placeholder Argon2 hashes in `init.sql` seed users with real hashes or remove seed accounts.
- [x] Reject oversized / malformed frames without tearing down the process (robustness).
- [ ] Align `PacketHandler` with all success paths (today mostly LOGIN/REGISTER; client often checks `responseCode` alone).

---



## 8. Testing (from `tests/TESTS.md`)

Highest-value gaps:

- [x] E2E: two clients, same room, MESSAGE push received (live `Server` + `Client` in `tests/func/`).
- [x] E2E: join / leave Lobby rules, unknown room `404`, logout/login, double-login rejected.
- [x] Cluster unit coverage: LOGIN / USER_CREATED / ROOM_JOIN / MESSAGE / ROOM_CREATED+ACL / anti-entropy DIGEST-PULL (`gossip_manager_test`).
- [x] Heartbeat: live pong keeps session; silent client timed out and presence cleared.
- [ ] Unit: `ThreadPool`, `config::parseArgs`, `PacketHandler` contract beyond login/register.
- [ ] Remove or wire `tests/class/` leftover (not built today).

---



## 9. Docs & cleanup

- [x] Layered architecture documented in [architecture.md](architecture.md); schema in [database.md](database.md); README points at both.
- [x] PowerShell helpers under `scripts/` (`build`, `run_server`, `run_client`, `run`, `run_cluster`).
- [x] README “not in this tree” list synced with shipped features (profile, rooms, TCP helpers, DB enums, allow_list).
- [ ] Decide product scope for later: WebSocket / shared remote DB.

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
- [ ] Live peer sockets on Ports (optional `GossipManager` snapshot getters).
- [ ] Push updates (Logger sink / session events) instead of timer-only where it matters.



### Client GUI

- [x] Qt client dashboard (`DashboardPage` / `MainPage`) beside console `ConsoleUI` (`--test`).
- [x] Screens: home/login/register, dashboard, join/create/delete room, messaging, profile, invite/kick, history.
- [x] Reuse `Client` / `PacketBuilder` / `PacketHandler` / `Network` — GUI is presentation only.
- [x] Show **pushed messages** live (pairs with §4 console push presentation).
- [x] GUI-thread rules: no widget updates from reader/network threads without queued signals.

---

