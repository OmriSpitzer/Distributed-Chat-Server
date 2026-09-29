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

- [x] `ThreadPool` removed. Accept, WebSocket accept, heartbeat, gossip, and each client session use dedicated threads.
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
- [x] PowerShell helpers under `scripts/` (`build`, `run_server`, `run_client`, `run`, `run_cluster`, `run_test`).
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



## 1. Design Patterns - Implementation **(goal)**

Ship the structural patterns below so client and server stay testable. Shared types (`IHealthCheck`, `HealthMonitor`) live in a common place; each process wires only the adapters it owns.

### Health checks (Adapter + Composite)

Shared `IHealthCheck` (Adapter target) and `HealthMonitor` (Composite). Same monitor type on client and server; each process registers different leaf adapters.

- [x] Define `IHealthCheck` → `HealthReport` (`name`, `Up` / `Degraded` / `Down`, optional detail).
- [x] `HealthMonitor` holds `IHealthCheck` children, exposes `checkAll()` / overall status (Composite; may itself implement `IHealthCheck`).
- [x] **Server adapters:** `DatabaseHealthAdapter`, `HeartbeatHealthAdapter`, `ServerHealthAdapter` (`isAlive` / listening) — register in `Server::start`.
- [x] **Client adapters:** `ClientHealthAdapter` (`Network` connected) — register in `Client::start`.
- [x] Do not register DB / Heartbeat adapters on the client (those objects do not live there).
- [x] Wire failover (§3) and GUI status to `HealthMonitor`, not ad-hoc `isAlive()` only.
- [x] Unit-test each adapter against live objects; test monitor rollup (one child Down → overall Down / Degraded).



### Logger

Façade + singleton: call sites keep `Logger::log*`; sinks register with `addLogger` (Client / Server startup). In-memory ring stays on the façade for the Qt Log panel. No file / remote sinks for now — do **not** replace static `Logger::log`* with DI.

- [x] `ILogger` interface for façade sinks (`log(const LogMessage&)`).
- [x] In-memory ring on `Logger` (default store for Qt Log panel).
- [x] `ConsoleLogger` sink; register via `addLogger` in Client / Server `start`.
- [x] `QtLogger` sink for live Log panel updates (register in `LogPanel`).
- [x] Facade fan-out tests (recording sink + `clearLoggers`); still use the singleton API.



## 3. Client failover — servers publish neighbors; clients reconnect **(goal)**

**Design:** Topology for client failover lives with the servers (they already gossip). Clients stay dumb: bootstrap from a seed, cache a directory, reconnect when the current node dies. Dead nodes cannot hand clients off; peers do not dial clients. A user who was already logged in stays logged in: the UI does not return to anonymous and does not ask for the password again. After the new socket is up, the client sends `LOGIN` and `ROOM_JOIN` from the in-memory session (username, password already held for this process, current room).

`Network::connect(host, port)` dials one endpoint. The client supervisor walks one failover list: endpoints the last `SERVER_DIRECTORY` reported as up, then the static seed (`--host`/`--port`, then `--servers`) that were not in that directory, with backoff and jitter. The connected endpoint stays at the front until the link fails. On boot this node should clear its own `online_users`. On the node the client lands on, a resume `LOGIN` takes the existing row when the recorded `node_id` is a process that has died (its `HELLO` boot id changed): `online_users` becomes `username → this node`, then that move is rumored. The row is not deleted first, so a third node cannot slip in a second login. A node that is still up (same boot id) still rejects the login as “user already logged in.”

### Ownership


| Who        | Owns                                                                                               | Does not                                            |
| ---------- | -------------------------------------------------------------------------------------------------- | --------------------------------------------------- |
| **Server** | Known **client-facing** neighbors (`host:port` chat TCP), pushed as a directory from gossip/health | Client TCP sessions on other nodes; dialing clients |
| **Client** | Seed + cached directory; reconnect / round-robin; resume in-memory user + room (no login prompt)   | Gossip `--peers` / `PEER_PORT`                      |


`--servers` / `NEIGHBOR_SERVERS` = client failover endpoints. `--peers` = server-to-server gossip only.

### Bootstrap (client / ops)

- [x] Config: `--servers host:port,host:port` (client listen ports, not gossip `--peers`). Keep `--host`/`--port` as single-endpoint shorthand.



### Server — publish directory

- [x] Each node tracks live **client** endpoints of cluster members (from gossip HELLO / peer health — not raw `--peers` gossip ports).
- [x] Push (or reply to) a lightweight **directory** packet of `host:port` chat endpoints to connected clients (on connect, on membership change, and/or periodically).
- [x] Refresh after login so the client’s cache matches who the new node believes is up.
- [ ] On resume `LOGIN`, if `online_users.node_id` is a node whose boot id changed, set that row to this node and rumor the move. Persist the last boot id seen per node. Same boot id means that node is still up: reject the login. Do not take a row from a bare reconnect with no `LOGIN`.



### Client — cache and reconnect

- [x] Merge seed + server directory into a failover list; prefer endpoints the last directory reported as up.
- [x] On connect failure, peer close, heartbeat miss / `isAlive() == false`, or failed `HealthMonitor` / Client adapter: try the next endpoint (round-robin or priority) with backoff + jitter.
- [x] Surface “reconnecting...” in the console and the Qt header while the supervisor dials the next endpoint. Session resume is not wired yet.
- [x] Cap reconnect storms; do not retry forever without UI cancel.
- [ ] When the new socket is up and the process still holds a logged-in user, send `LOGIN` then `ROOM_JOIN` for the room they were in. Keep the same user and room on screen. Do not ask for the password again. A guest with no account stays a guest.
- [x] Wire reconnect triggers to `HealthMonitor` (pairs with §1), not ad-hoc checks only.



### Verify

- [ ] E2E: kill node A while a logged-in client is in a room; client uses the cached directory, lands on node B, still shows the same user and room (no login prompt), and receives MESSAGE again (pairs with §7).



## 4. Website integration **(goal)**

Desktop Qt/console stay primary for chat; the website is the same kind of client, not a second gossip peer. The browser talks to a node on `--ws-port`. There is no gateway process and no website cookie or JWT.

- [x] Do **not** put REST inside `PacketProcessor`; it still sees only `Packet`.
- [x] WebSocket listener on `chat_server` (`--ws-port`): `Server` owns `WebConnection` and runs its accept loop beside `ConnectionManager`. Each binary frame payload is one existing length-prefixed `Packet`. The socket is a `ConnectionManager` session. Qt clients stay on `--port`. Gossip stays on `--peer-port`. `wss` terminates at the reverse proxy.
- [x] Browser dashboard in `web/` uses that socket for the same actions as `DashboardPage`: register, login, logout, profile, join / leave / create / invite / kick / delete, send, and history. Heartbeat `ping` is answered with `pong`. The chat session is the WebSocket; there is no separate website login cookie.
- [x] Website auth is that same `Packet` session. Chat nodes keep Argon2id. The page does not store a second credential.
- [x] Threat model (README “Browser WebSocket”): the password rides in the `Packet` and is not written to browser storage.



## 5. AWS database for the website **(goal)**

Chat traffic stays on per-node SQLite + gossip. Website gets its own remote DB for accounts/metadata that the browser product needs.

- [ ] Provision managed DB (e.g. **Amazon RDS** PostgreSQL or Aurora) for website schema only — users mirror / profile cache, sessions, audit log, feature flags — **not** live `messages` gossip log.
- [ ] Sync strategy (pick one and document):
  - **A.** The chat node stays authoritative for passwords (Argon2id on REGISTER/UPDATE). A website DB, if added, mirrors those events from the node; or
  - **B.** Website DB stores website-only accounts; link username to chat user after first successful cluster login.
- [ ] Never point `DatabaseManager` / `init.sql` at RDS for node local chat — keep SQLite on each Windows node for low-latency rumor apply.
- [ ] Optional later: S3 for exports / attachments; Secrets Manager for the website DB URL; IAM auth for RDS.
- [ ] Migrations (Flyway/Liquibase or SQL scripts) versioned beside website code; separate from `src/database/init.sql`.
- [ ] Backup / PITR on RDS; chat SQLite backup remains per-node (`data/*.db`) + gossip recovery.



## 6. Gossip payload — object-oriented & scalable

Five fixed fields (`type`, `eventId`, `username`, `content`, `field5`) already limit ROOM_CREATED+ACL and force overloading `field5`.

- [ ] Versioned envelope: `{ version, type, eventId, body }` with typed body structs per event (LOGIN, MESSAGE, ROOM_CREATED, …).
- [ ] Keep length-prefixed binary or add JSON/CBOR for website debugging — same semantic types either way.
- [ ] Backward-compatible decode: v1 five-field still accepted for one release; peers advertise version on HELLO.
- [ ] Unit tests for every event type round-trip; reject unknown version cleanly.



## 7. Testing — more kinds **(goal)**

Strong unit surface (~340 cases); gaps are live dual-process, failover, GUI, and load (see `tests/TESTS.md`).

### 7.1 Unit / component (still missing)

- [ ] `config::parseArgs` / `parsePort` / `parsePeers` (and new `--servers`).
- [ ] `PacketHandler` for every RPC type, not only LOGIN/REGISTER/UPDATE_USER.
- [ ] `IHealthCheck` adapters + `HealthMonitor` rollup (§1).
- [ ] `allow_list` CRUD edges as dedicated `DatabaseManager` cases.
- [ ] Clear this node's `online_users` on boot (`DatabaseManager::clearNodePresence`, `Server::start`).
- [ ] Resume `LOGIN` adopts `online_users` when the row’s node has a new boot id (`node_id` becomes this node, rumor the move). Same boot id still rejects a second login (§3).



### 7.2 Integration / E2E (live `Server` + `Client`)

- [ ] Disconnect while logged in → presence cleared + LOGOUT rumor.
- [ ] Client reconnect after server restart resumes the in-memory user and room. The UI does not drop to anonymous or ask for the password again.
- [ ] Invite + private join allow-list across live client.
- [ ] Full-stack heartbeat timeout clears presence (`Network` + `Heartbeat` together).



### 7.3 Cluster / multi-process

- [ ] Two `chat_server` processes: LOGIN on A → B `online_users`; USER_CREATED cross-login; duplicate login rejected across nodes.
- [ ] ROOM_JOIN / MESSAGE / ACL over real peer sockets (not only in-process gossip fixtures).
- [ ] `MAX_EVENT_LOG` drop: late PULL cannot resurrect ids.
- [ ] **Failover E2E:** stop node A; logged-in client fails over to B still as that user, in the same room, with no login prompt; messaging resumes (§3).



### 7.4 New test kinds

- [ ] **Contract / golden** tests for `Serializer` + gossip envelope bytes (fixtures under `tests/fixtures/`).
- [ ] **Property / fuzz** light: random oversize/malformed frames must not kill `handleClient`.
- [ ] **Load / soak** (optional Catch2 `[slow]` or separate script): N clients, M msg/s, gossip lag bounds.
- [ ] **Chaos:** kill gossip peer mid-rumor; anti-entropy heals within digest interval.
- [ ] **GUI smoke** (optional): Qt Test or scripted `--test` console paths for login → join → send.
- [ ] CI matrix: Debug/Release + `ctest --output-on-failure`; tag `[slow]` / `[cluster]` excluded from default PR job if flaky.



## 8. GUI additions **(goal)**

Qt exists (client dashboard, server Ports/Users/Rooms/Log) but panels are timer-polled; Ports shows gossip seeds + live client endpoints from HELLO; no failover UX.

### Server GUI

- [ ] Event-driven refresh (subscribe to Logger / connection / room events) — keep timer as fallback.
- [ ] Ports: live peer table (node id, socket, last HELLO/DIGEST time, up/down).
- [ ] Live peer sockets on Ports panel (`GossipManager` snapshot getters — connected node ids, not only `config::PEERS`).
- [ ] Users: show which **node** holds the session (from `online_users`), not only local `getSessions()`.
- [ ] Gossip/event-log panel: recent rumor types, digest size, dropped ids near `MAX_EVENT_LOG`.
- [ ] Health panel or Ports footer: `HealthMonitor` reports (DB / Heartbeat / Server).
- [ ] Controls: drain listeners, “clear stale online”, copy node config.



### Client GUI

- [ ] Connection status strip: connected host:port, reconnecting, failed over to X (`HealthMonitor` / Client adapter).
- [ ] Server picker / auto-failover progress (ties to §3); manual “switch server”.
- [ ] Unread badge / scroll-to-bottom; optional toast on kick/invite/ROOM_LIST change.
- [ ] Dark/light or denser chat layout without putting logic in widgets (still `Client` only).
- [ ] Accessibility: tab order, high-contrast errors, no updates off the GUI thread (keep queued signals).



## 9. Transport & API surface (website)

The website uses the chat node's `--ws-port`. There is no gateway and no REST API on the node.

- [x] `--ws-port` on `chat_server` (§4): `Server` owns `WebConnection`; binary frames carry the same `Packet` bytes.
- [x] `PacketProcessor` sees only `Packet` (§4).



## 10. Docs & ops

- [ ] Update README architecture diagram: clients → multi-server failover; browser → `--ws-port`; AWS RDS for website only.
- [ ] Runbook: node crash, client failover, RDS failover, gossip partition.
- [ ] Sync `tests/TESTS.md` checkboxes when §7 cases land; fix doc typo link `FUTURE_WORKs.md` → `FUTURE_WORK.md`.
- [ ] Version badge / changelog note for 3.0.0 when shipping.