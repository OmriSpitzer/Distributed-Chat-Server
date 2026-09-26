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



## 1. Design Patterns - Implementation **(goal)**

Ship the structural patterns below so client and server stay testable. Shared types (`IHealthCheck`, `HealthMonitor`) live in a common place; each process wires only the adapters it owns.

### Design patterns on the server

- [ ] **Sidecar** — optional side process / plugin for dynamic logic (metrics scrape, audit export, hot-config) without bloating `PacketProcessor`.
- [ ] **Strategy** for join / ACL / ADMIN gates so room privacy rules are swappable without touching TCP.
- [ ] **Observer / event bus** for GUI + website: domain events (`UserOnline`, `MessagePosted`, `RoomCreated`) instead of Qt `QTimer` polls only.
- [ ] Keep singletons (`DatabaseManager`, `RoomManager`) or migrate to DI owned by `Server` if website tests need fakes.



### Health checks (Adapter + Composite)

Shared `IHealthCheck` (Adapter target) and `HealthMonitor` (Composite). Same monitor type on client and server; each process registers different leaf adapters.

- [x] Define `IHealthCheck` → `HealthReport` (`name`, `Up` / `Degraded` / `Down`, optional detail).
- [x] `HealthMonitor` holds `IHealthCheck` children, exposes `checkAll()` / overall status (Composite; may itself implement `IHealthCheck`).
- [x] **Server adapters:** `DatabaseHealthAdapter`, `HeartbeatHealthAdapter`, `ServerHealthAdapter` (`isAlive` / listening) — register in `Server::start`.
- [ ] **Client adapters:** `ClientHealthAdapter` (`Network` connected) — register in `Client::start`.
- [x] Do not register DB / Heartbeat adapters on the client (those objects do not live there).
- [ ] Wire failover (§3) and GUI status to `HealthMonitor`, not ad-hoc `isAlive()` only.
- [ ] Unit-test each adapter with fakes; test monitor rollup (one child Down → overall Down / Degraded).



### Logger

Façade + singleton: call sites keep `Logger::log*`; sinks register with `addLogger` (Client / Server startup). In-memory ring stays on the façade for the Qt Log panel. No file / remote sinks for now — do **not** replace static `Logger::log`* with DI.

- [x] `ILogger` interface for façade sinks (`log(const LogMessage&)`).
- [x] In-memory ring on `Logger` (default store for Qt Log panel).
- [x] `ConsoleLogger` sink; register via `addLogger` in Client / Server `start`.
- [x] `QtLogger` sink for live Log panel updates (register in `LogPanel`).
- [x] Facade fan-out tests (recording sink + `clearLoggers`); still use the singleton API.



## 2. Architecture — cluster presence & node lifecycle

Today each node has its own SQLite; gossip syncs events. Crash leaves stale `online_users` (no clear-on-boot).

- [ ] Clear cluster presence on node boot (`online_users` must not survive a crash as “still online”); rumor peer cleanup or local wipe + HELLO.
- [ ] On graceful `Server::stop`, clear local sessions / rumor LOGOUT for still-connected users.
- [ ] Live peer sockets on Ports panel (`GossipManager` snapshot getters — connected node ids, not only `config::PEERS`).
- [ ] Document “source of truth”: per-node SQLite remains chat store; website/AWS is a **separate** account/admin surface (see §5–§6), not a replacement for gossip.
- [ ] Optional: node readiness flag (accept clients only after DB open + gossip listen + presence wipe).



## 3. Client failover — servers publish neighbors; clients reconnect **(goal)**

**Design:** Topology for client failover lives with the servers (they already gossip). Clients stay dumb: bootstrap from a seed, cache a directory, reconnect when the current node dies, re-auth. Dead nodes cannot hand clients off; peers do not dial clients.

`Network::connect` still uses a single `config::SERVER_HOST` / `PORT` today. Config seed list exists; no runtime directory, no auto-relogin after hop.

### Ownership


| Who        | Owns                                                                                               | Does not                                            |
| ---------- | -------------------------------------------------------------------------------------------------- | --------------------------------------------------- |
| **Server** | Known **client-facing** neighbors (`host:port` chat TCP), pushed as a directory from gossip/health | Client TCP sessions on other nodes; dialing clients |
| **Client** | Seed + cached directory; reconnect / round-robin; re-`LOGIN` + re-`ROOM_JOIN`                      | Gossip `--peers` / `PEER_PORT`                      |


`--servers` / `NEIGHBOR_SERVERS` = client failover endpoints. `--peers` = server-to-server gossip only.

### Bootstrap (client / ops)

- [x] Config: `--servers host:port,host:port` (client listen ports, not gossip `--peers`). Keep `--host`/`--port` as single-endpoint shorthand.
- [ ] Always keep a static seed (CLI / scripts / DNS) — directory can go stale if the last node dies before a push; do not hardcode only localhost demos.



### Server — publish directory

- [x] Each node tracks live **client** endpoints of cluster members (from gossip HELLO / peer health — not raw `--peers` gossip ports).
- [x] Push (or reply to) a lightweight **directory** packet of `host:port` chat endpoints to connected clients (on connect, on membership change, and/or periodically).
- [ ] Directory is **hints**, not trust: clients must still LOGIN/auth on the next node.
- [x] Refresh after login so the client’s cache matches who the new node believes is up.



### Client — cache and reconnect

- [ ] Merge seed + server directory into a failover list; prefer endpoints the last directory reported as up.
- [ ] On connect failure, peer close, heartbeat miss / `isAlive() == false`, or failed `HealthMonitor` / Client adapter: try the next endpoint (round-robin or priority) with backoff + jitter.
- [ ] After hop: restore session — re-`LOGIN` (or guest reconnect), re-`ROOM_JOIN` current room, drain/clear stale chat queue; surface “reconnecting…” in console + Qt.
- [ ] Cap reconnect storms; do not retry forever without UI cancel.
- [ ] Wire reconnect triggers to `HealthMonitor` (pairs with §1), not ad-hoc checks only.



### Verify

- [ ] E2E: kill node A while client is in a room; client uses cached directory, lands on node B, re-auths, and receives MESSAGE again (pairs with §7).
- [ ] Optional later: LB / gateway (§4) so clients hit one address and may not need a neighbor list — does not replace this path for direct TCP clients.



## 4. Website integration **(goal)**

Desktop Qt/console stay primary for chat; website is admin + light client / status, not a second gossip peer.

- [ ] Thin **gateway** (separate process or sidecar): REST and/or WebSocket for domain ops (login, rooms list, history read, optional send).
- [ ] Do **not** put HTTP inside `server_lib` packet path; gateway talks TCP/`Packet` to a chosen node.
- [ ] Public pages: cluster status (nodes up/down), room directory (public only), optional read-only message feed.
- [ ] Auth pages: register / login against chat cluster (via gateway); session cookie/JWT for website only — chat nodes keep Argon2id + binary sessions.
- [ ] Admin UI: kick/invite/delete room (ADMIN), view online users across nodes (aggregated from gateway polls or events).
- [ ] CORS / TLS termination at reverse proxy (nginx / ALB); chat servers stay on private ports.
- [ ] Document threat model: website creds ≠ wire `Packet` auth; no plaintext passwords in browser storage beyond normal session practice.



## 5. AWS database for the website **(goal)**

Chat traffic stays on per-node SQLite + gossip. Website gets its own remote DB for accounts/metadata that the browser product needs.

- [ ] Provision managed DB (e.g. **Amazon RDS** PostgreSQL or Aurora) for website schema only — users mirror / profile cache, sessions, audit log, feature flags — **not** live `messages` gossip log.
- [ ] Sync strategy (pick one and document):
  - **A.** Gateway writes website DB on REGISTER/UPDATE; chat nodes remain authoritative for passwords via cluster login API; or
  - **B.** Website DB stores website-only accounts; link username to chat user after first successful cluster login.
- [ ] Never point `DatabaseManager` / `init.sql` at RDS for node local chat — keep SQLite on each Windows node for low-latency rumor apply.
- [ ] Optional later: S3 for exports / attachments; Secrets Manager for gateway DB URL; IAM auth for RDS.
- [ ] Migrations (Flyway/Liquibase or SQL scripts) versioned beside website code; separate from `src/database/init.sql`.
- [ ] Backup / PITR on RDS; chat SQLite backup remains per-node (`data/*.db`) + gossip recovery.



## 6. Gossip payload — object-oriented & scalable

Five fixed fields (`type`, `eventId`, `username`, `content`, `field5`) already limit ROOM_CREATED+ACL and force overloading `field5`.

- [ ] Versioned envelope: `{ version, type, eventId, body }` with typed body structs per event (LOGIN, MESSAGE, ROOM_CREATED, …).
- [ ] Keep length-prefixed binary or add JSON/CBOR for website/gateway debugging — same semantic types either way.
- [ ] Backward-compatible decode: v1 five-field still accepted for one release; peers advertise version on HELLO.
- [ ] Unit tests for every event type round-trip; reject unknown version cleanly.



## 7. Testing — more kinds **(goal)**

Strong unit surface (~340 cases); gaps are live dual-process, failover, GUI, gateway, and load (see `tests/TESTS.md`).

### 7.1 Unit / component (still missing)

- [ ] `config::parseArgs` / `parsePort` / `parsePeers` (and new `--servers`).
- [ ] `ThreadPool` enqueue / shutdown (even if unused for session I/O).
- [ ] `PacketHandler` for every RPC type, not only LOGIN/REGISTER/UPDATE_USER.
- [ ] `IHealthCheck` adapters + `HealthMonitor` rollup (§1).
- [ ] `allow_list` CRUD edges as dedicated `DatabaseManager` cases.
- [ ] Clear-`online_users`-on-boot once implemented.



### 7.2 Integration / E2E (live `Server` + `Client`)

- [ ] Disconnect while logged in → presence cleared + LOGOUT rumor.
- [ ] Client reconnect after server restart (anonymous until login).
- [ ] Invite + private join allow-list across live client.
- [ ] Full-stack heartbeat timeout clears presence (`Network` + `Heartbeat` together).



### 7.3 Cluster / multi-process

- [ ] Two `chat_server` processes: LOGIN on A → B `online_users`; USER_CREATED cross-login; duplicate login rejected across nodes.
- [ ] ROOM_JOIN / MESSAGE / ACL over real peer sockets (not only in-process gossip fixtures).
- [ ] `MAX_EVENT_LOG` drop: late PULL cannot resurrect ids.
- [ ] **Failover E2E:** stop node A; client fails over to B; messaging resumes (§3).



### 7.4 New test kinds

- [ ] **Contract / golden** tests for `Serializer` + gossip envelope bytes (fixtures under `tests/fixtures/`).
- [ ] **Property / fuzz** light: random oversize/malformed frames must not kill `handleClient`.
- [ ] **Load / soak** (optional Catch2 `[slow]` or separate script): N clients, M msg/s, gossip lag bounds.
- [ ] **Chaos:** kill gossip peer mid-rumor; anti-entropy heals within digest interval.
- [ ] **Website/gateway API** tests (HTTP status + JSON schema) once §4 ships — separate from Catch2 C++ if stack is different.
- [ ] **GUI smoke** (optional): Qt Test or scripted `--test` console paths for login → join → send.
- [ ] CI matrix: Debug/Release + `ctest --output-on-failure`; tag `[slow]` / `[cluster]` excluded from default PR job if flaky.



## 8. GUI additions **(goal)**

Qt exists (client dashboard, server Ports/Users/Rooms/Log) but panels are timer-polled; Ports shows gossip seeds + live client endpoints from HELLO; no failover UX.

### Server GUI

- [ ] Event-driven refresh (subscribe to Logger / connection / room events) — keep timer as fallback.
- [ ] Ports: live peer table (node id, socket, last HELLO/DIGEST time, up/down).
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



### Optional third UI

- [ ] Website admin/status pages (§4) styled separately; do not embed Qt WebEngine unless product requires it.



## 9. Transport & API surface (website)

- [ ] WebSocket and/or REST gateway implementing website ops (§4).
- [ ] OpenAPI (REST) or schema doc for gateway; map HTTP errors to existing `200`/`400`/`404`/`500` meanings.
- [ ] Rate limits on gateway register/login; same Argon2id verification path via cluster, not a second hash scheme.
- [ ] “RESTfulness on APIs” = gateway resources (`/rooms`, `/rooms/{id}/messages`), not rewriting binary `Packet` into REST inside each node.



## 10. Docs & ops

- [ ] Update README architecture diagram: clients → multi-server failover; website → gateway → node; AWS RDS for website only.
- [ ] Runbook: node crash, client failover, RDS failover, gossip partition.
- [ ] Sync `tests/TESTS.md` checkboxes when §7 cases land; fix doc typo link `FUTURE_WORKs.md` → `FUTURE_WORK.md`.
- [ ] Version badge / changelog note for 3.0.0 when shipping.