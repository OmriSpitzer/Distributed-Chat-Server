# Improvements & things to do

Backlog for the distributed chat system. Priority is approximate; items marked **(goal)** are explicit design targets.

---

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

---



## 3. TCP layer — clearer transport **(goal)**

Framing works (`socket_io` + `Serializer`), but sockets are still raw `int` / cast `SOCKET`, split across `ConnectionManager`, `Network`, and gossip.

- [x] Introduce a small, explicit TCP API (listen / accept / connect / read-frame / write-frame / close) with one socket type end-to-end — no `int` ↔ `SOCKET` casts at call sites.
- [x] Keep packet framing (`[u32 BE size][payload]`) only in that layer; higher layers deal only in `Packet`.
- [x] Align client `Network` and server `ConnectionManager` on the same helpers.

---



## 4. Client product gaps

- [ ] **Update profile** — dashboard choice 1 is still a TODO (`Client::showDashboard`).
- [ ] **Room directory** — console should list available rooms before join (`ConsoleUI::showJoinRoom`).
- [x] **Validate room exists** before sending `ROOM_JOIN` (or surface clear `404` from server).
- [ ] **Message history** — server has `loadHistory`; client never requests or displays it after join.
- [ ] **Pushed messages in UI** — reader logs room pushes; improve console presentation (“change visuals” TODO).
- [ ] Refresh outdated `CLIENT.md` (still claims TCP stubs).

---



## 5. Rooms & authorization

- [ ] Wire protocol for `createRoom` / `deleteRoom` (RoomManager APIs exist; no client packet types yet).
- [ ] Enforce **PRIVATE** vs **PUBLIC** (schema supports it; join path does not).
- [ ] Enforce **ADMIN / USER / GUEST** privileges (seed has ADMIN; no permission checks).
- [x] Unique **email** constraint coverage and clear client errors (username uniqueness is stronger today).

---



## 6. Server runtime & architecture

- [ ] `ThreadPool`: use it for a clear reason, or delete it. Dedicated session threads are fine; an unused pool is not.
- [ ] If kept: finish the hard-shutdown TODO; if removed: drop construction/shutdown from `Server`.
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

- [ ] E2E: two clients, same room, MESSAGE push received.
- [ ] E2E: join / leave Lobby rules, unknown room `404`, logout/login, double-login rejected.
- [x] Cluster: LOGIN / USER_CREATED / ROOM_JOIN / MESSAGE / anti-entropy DIGEST-PULL across two nodes.
- [x] Heartbeat: live pong keeps session; silent client timed out and presence cleared.
- [ ] Unit: `ThreadPool`, `config::parseArgs`, `PacketHandler` contract beyond login/register.
- [ ] Remove or wire `tests/class/` leftover (not built today).

---



## 9. Docs & cleanup

- [ ] Sync `SERVER.md` / `CLIENT.md` / `MODELS.md` with the current layered architecture (`architecture.md` is closer to truth).
- [x] README “not in this tree” list: track profile, room directory, history UI, clearer TCP, DB enums as they land.
- [ ] Decide product scope for later: WebSocket / GUI clients, shared remote DB (explicitly out of tree today).

---



## Suggested order

1. Security + presence-on-boot (gossip hash-only, seed hashes, clear `online_users`)
2. Persistence mapper cleanup + DB enum `CHECK`s
3. Clearer TCP wrapper shared by client and server
4. Client room list + history + profile
5. AuthZ (roles / private rooms)
6. Use or delete `ThreadPool`
7. Fill E2E / cluster test gaps

