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
- [x] Wire failover (§2) and GUI status to `HealthMonitor`, not ad-hoc `isAlive()` only.
- [x] Unit-test each adapter against live objects; test monitor rollup (one child Down → overall Down / Degraded).



### Logger

Façade + singleton: call sites keep `Logger::log*`; sinks register with `addLogger` (Client / Server startup). In-memory ring stays on the façade for the Qt Log panel. No file / remote sinks for now — do **not** replace static `Logger::log`* with DI.

- [x] `ILogger` interface for façade sinks (`log(const LogMessage&)`).
- [x] In-memory ring on `Logger` (default store for Qt Log panel).
- [x] `ConsoleLogger` sink; register via `addLogger` in Client / Server `start`.
- [x] `QtLogger` sink for live Log panel updates (register in `LogPanel`).
- [x] Facade fan-out tests (recording sink + `clearLoggers`); still use the singleton API.



## 2. Client failover — servers publish neighbors; clients reconnect **(goal)**

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

- [x] E2E: kill node A while a logged-in client is in a room; client uses the cached directory, lands on node B, still shows the same user and room (no login prompt), and receives MESSAGE again (pairs with §4).



## 3. Website integration **(goal)**

Desktop Qt/console stay primary for chat; the website is the same kind of client, not a second gossip peer. The browser talks to a node on `--ws-port`. There is no gateway process and no website cookie or JWT.

- [x] Do **not** put REST inside `PacketProcessor`; it still sees only `Packet`.
- [x] WebSocket listener on `chat_server` (`--ws-port`): `Server` owns `WebConnection` and runs its accept loop beside `ConnectionManager`. Each binary frame payload is one existing length-prefixed `Packet`. The socket is a `ConnectionManager` session. Qt clients stay on `--port`. Gossip stays on `--peer-port`. `wss` terminates at the reverse proxy.
- [x] Browser dashboard in `web/` uses that socket for the same actions as `DashboardPage`: register, login, logout, profile, join / leave / create / invite / kick / delete, send, and history. Heartbeat `ping` is answered with `pong`. The chat session is the WebSocket; there is no separate website login cookie.
- [x] Website auth is that same `Packet` session. Chat nodes keep Argon2id. The page does not store a second credential.
- [x] Threat model (README “Browser WebSocket”): the password rides in the `Packet` and is not written to browser storage.



## 4. Testing — more kinds **(goal)**

Unit coverage is broad; the gaps are live dual-process, failover, GUI, and load (see `tests/TESTS.md`).

- [x] Test argument parsing, including `--servers`.
- [x] Test every packet handler, not only login, register, and profile update.
- [x] Test health adapters and the monitor rollup (§1).
- [x] Test allow-list add, check, and remove.
- [x] Boot clears this node's online users.
- [x] Reconnect takes a presence row only when that node is not a live peer (§2).
- [x] Failover: stop node A; the client lands on B as the same user, in the same room (§2).



## 5. Client GUI additions **(goal)**

- [x] Connection status strip shows `Connected host:port` on the Qt subtitle and the browser header.
- [x] Unread badge / scroll-to-bottom on the Qt transcript and the browser dashboard. A scrolled-up view stays put; "N new" jumps to the latest line.
- [x] Dark/light or denser chat layout without putting logic in widgets (still `Client` only). The browser dashboard uses the same pair of controls; `App` holds the choice and the panels only paint it.



## 6. Transport & API surface (website)

The website uses the chat node's `--ws-port`. There is no gateway and no REST API on the node.

- [x] `--ws-port` on `chat_server` (§3): `Server` owns `WebConnection`; binary frames carry the same `Packet` bytes.
- [x] `PacketProcessor` sees only `Packet` (§3).



## 7. Docs & ops

- [x] Update README architecture diagram: clients → multi-server failover; browser → `--ws-port`; AWS RDS for website only.
- [x] Sync `tests/TESTS.md` checkboxes when §4 cases land; fix doc typo link `FUTURE_WORKs.md` → `FUTURE_WORK.md`.
- [x] Version badge / changelog note for 3.0.0 when shipping.