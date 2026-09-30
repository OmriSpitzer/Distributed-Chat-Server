# Improvements & things to do

Backlog for the distributed chat system. Priority is approximate; items marked **(goal)** are explicit design targets.

**Docs:** [README.md](README.md) · [architecture.md](architecture.md) · [database.md](database.md) · [tests/TESTS.md](tests/TESTS.md)

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

**Design:** Topology for client failover lives with the servers (they already gossip). Clients stay dumb: bootstrap from a seed, cache a directory, reconnect when the current node dies. Dead nodes cannot hand clients off; peers do not dial clients. A user who was already logged in stays logged in: the UI does not return to anonymous and does not ask for the password again. After the new socket is up, the client sends `RECONNECT` (username and current room already held for this process). The password is not stored.

`Network::connect(host, port)` dials one endpoint. The client supervisor walks one failover list: endpoints the last `SERVER_DIRECTORY` reported as up, then the static seed (`--host`/`--port`, then `--servers`) that were not in that directory, with backoff and jitter. The connected endpoint stays at the front until the link fails. On boot this node clears `online_users` rows whose `node_id` is itself. On the node the client lands on, `RECONNECT` looks the user up by username and takes the existing presence row when that `node_id` is not a live peer: `online_users.node_id` and every `membership.node_id` for the user become this node, and that move is rumored as `RECONNECT`. The row is not deleted first, so a third node cannot slip in a second login. A `node_id` that is still a live peer rejects `RECONNECT` as “user already logged in.” A guest with no account stays a guest.

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
- [x] On `RECONNECT`, if `online_users.node_id` is not a live peer, set that row and the user's `membership.node_id` values to this node and rumor the move. A live peer with that `node_id` rejects the resume. Do not take a row from a bare TCP reconnect with no `RECONNECT`.



### Client — cache and reconnect

- [x] Merge seed + server directory into a failover list; prefer endpoints the last directory reported as up.
- [x] On connect failure, peer close, heartbeat miss / `isAlive() == false`, or failed `HealthMonitor` / Client adapter: try the next endpoint (round-robin or priority) with backoff + jitter.
- [x] Surface “reconnecting...” in the console and the Qt header while the supervisor dials the next endpoint.
- [x] Cap reconnect storms; do not retry forever without UI cancel.
- [x] When the new socket is up and the process still holds a logged-in user, send `RECONNECT` for the room they were in. Keep the same user and room on screen. Do not ask for the password again. A guest with no account stays a guest.
- [x] Wire reconnect triggers to `HealthMonitor` (pairs with §1), not ad-hoc checks only.



### Verify

- [x] E2E: kill node A while a logged-in client is in a room; client uses the cached directory, lands on node B, still shows the same user and room (no login prompt), and receives MESSAGE again (pairs with §7).



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
- [x] Clear this node's `online_users` on boot (`DatabaseManager::clearNodePresence`, `Server::start`).
- [x] `RECONNECT` adopts `online_users` when the recorded `node_id` is not a live peer (`node_id` becomes this node, membership moves with it, rumor the move). A live peer with that `node_id` still rejects a second session (§3).



### 7.2 Integration / E2E (live `Server` + `Client`)

- [ ] Disconnect while logged in → presence cleared + LOGOUT rumor.
- [ ] Client reconnect after server restart resumes the in-memory user and room. The UI does not drop to anonymous or ask for the password again.
- [ ] Invite + private join allow-list across live client.
- [ ] Full-stack heartbeat timeout clears presence (`Network` + `Heartbeat` together).



### 7.3 Cluster / multi-process

- [ ] Two `chat_server` processes: LOGIN on A → B `online_users`; USER_CREATED cross-login; duplicate login rejected across nodes.
- [ ] ROOM_JOIN / MESSAGE / ACL over real peer sockets (not only in-process gossip fixtures).
- [ ] `MAX_EVENT_LOG` drop: late PULL cannot resurrect ids.
- [x] **Failover E2E:** stop node A; logged-in client fails over to B still as that user, in the same room, with no login prompt; messaging resumes (§3).



### 7.4 New test kinds

- [ ] **Contract / golden** tests for `Serializer` + gossip envelope bytes (fixtures under `tests/fixtures/`).
- [ ] **Property / fuzz** light: random oversize/malformed frames must not kill `handleClient`.
- [ ] **Load / soak** (optional Catch2 `[slow]` or separate script): N clients, M msg/s, gossip lag bounds.
- [ ] **Chaos:** kill gossip peer mid-rumor; anti-entropy heals within digest interval.
- [ ] **GUI smoke** (optional): Qt Test or scripted `--test` console paths for login → join → send.
- [ ] CI matrix: Debug/Release + `ctest --output-on-failure`; tag `[slow]` / `[cluster]` excluded from default PR job if flaky.



## 8. Client GUI additions **(goal)**

### Client GUI

- [ ] Connection status strip: connected host:port, reconnecting, failed over to X (`HealthMonitor` / Client adapter).
- [ ] Server picker / auto-failover progress (ties to §3); manual “switch server”.
- [ ] Unread badge / scroll-to-bottom; optional toast on kick/invite/ROOM_LIST change.
- [x] Dark/light or denser chat layout without putting logic in widgets (still `Client` only). The browser dashboard uses the same pair of controls; `App` holds the choice and the panels only paint it.
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