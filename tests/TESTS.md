# Test catalog

Catch2 cases wired in `CMakeLists.txt` (`catch_discover_tests`). Run with CTest after a CMake build.

Totals: **24 executables**, **306** `TEST_CASE`s.

Catch2 tags used throughout: `[flow]` typical happy path, `[edge]` invalid/empty/boundary, `[thread]` / `[concurrent]` races, `[slow]` heartbeat waits.

`tests/class/` is leftover and is **not** built.

---

## Utils — models and wire format

| Executable | File | Cases |
|---|---|---|
| `user_test` | `tests/utils/user_test.cpp` | 10 |
| `room_test` | `tests/utils/room_test.cpp` | 10 |
| `message_test` | `tests/utils/message_test.cpp` | 12 |
| `packet_test` | `tests/utils/packet_test.cpp` | 11 |
| `log_message_test` | `tests/utils/log_message_test.cpp` | 12 |
| `logger_test` | `tests/utils/logger_test.cpp` | 12 |
| `serializer_test` | `tests/utils/serializer_test.cpp` | 8 |
| `socket_io_test` | `tests/utils/socket_io_test.cpp` | 10 |
| `gossip_payload_test` | `tests/utils/gossip_payload_test.cpp` | 11 |

### User (`[user]`)

- User constructor stores metadata
- User setters mutate fields
- User equality is by email only
- User stream output
- User typeToString
- User stringToType
- User type conversion round-trip
- User anonymousUser
- User serialize / deserialize
- User deserialize rejects invalid input

### Room (`[room]`)

- Room constructor stores metadata
- Successive rooms receive unique ids
- Room copy keeps metadata
- Room roomTypeToString
- Room stringToRoomType
- Room type conversion round-trip
- Room privacyToString
- Room stringToPrivacy
- Room privacy conversion round-trip
- Room stream output includes name, type, and privacy

### Message (`[message]`)

- Message constructor stores sender, receiver, and content
- Message content edge cases
- Message timestamp is set near construction time
- Successive messages receive unique ids
- Message id format includes node boot time and seq
- Message reconstruct ctor keeps stored id and timestamp
- Message equality compares id and timestamp
- Message ordering is by timestamp
- Message same-timestamp different-id ordering
- Message stream output includes users content id and timestamp
- Message works with all user types and empty users
- Message self-send and stores independent user copies

### Packet (`[packet]`)

- Default Packet has DEFAULT type and empty fields
- Packet constructor stores all fields
- Packet constructor default arguments
- Packet field edge cases
- Packet responseCode edge cases
- Packet timestamp is set near construction time
- Packet copy preserves all fields
- Packet packetTypeToString
- Packet stringToPacketType
- Packet type conversion round-trip
- Packet public fields are independently mutable

### LogMessage (`[log_message]`)

- LogMessage constructor stores source, message, and type
- LogMessage source edge cases
- LogMessage message content edge cases
- LogMessage timestamp is set near construction time
- Successive log messages receive unique ids
- Heartbeat log messages receive unique ids
- LogMessage equality compares by id only
- LogMessage typeToString
- LogMessage stringToType
- LogMessage type conversion round-trip
- LogMessage stream output
- LogMessage getters return stored fields

### Logger (`[logger]`)

- Logger is empty after clear
- Logger stores each log type
- Logger source and message edge cases
- Logger keeps insertion order
- Logger getMessage bounds
- Logger clear removes all messages
- Logger evicts oldest messages past kMaxMessages
- Logger stream output
- Logger is a singleton
- Logger heartbeat messages receive unique ids
- Logger mixed types keep distinct ids and fields
- Logger size tracks additions and clear

### Serializer (`[serializer]`)

- Serializer round-trip all packet types
- Serializer empty and whitespace fields
- Serializer unicode and special characters
- Serializer timestamp and responseCode extremes
- Serializer serialize rejects invalid type
- Serializer serialize rejects oversize payload
- Serializer deserialize rejects bad frames
- Serializer double round-trip is stable

### socket_io (`[socket_io]`)

- socket_io listenTo binds and accepts
- socket_io connectTo reaches a listener
- socket_io sendExact / recvExact edges
- socket_io writePacket frames match Serializer
- socket_io readPacket restores Serializer frames
- socket_io writePacket / readPacket round-trip edges
- socket_io writePacket rejects invalid packets
- socket_io readPacket rejects bad frames
- socket_io peer close and invalid sockets
- socket_io back-to-back packets

### gossip_payload (`[gossip_payload]`)

- gossip_payload round-trip typical events
- gossip_payload round-trip empty fields
- gossip_payload round-trip pipes and whitespace
- gossip_payload round-trip unicode and null bytes
- gossip_payload round-trip large content
- gossip_payload wire size is 20 plus field lengths
- gossip_payload decode rejects empty short truncated
- gossip_payload decode rejects trailing junk and bad lengths
- gossip_payload eventId full and partial
- gossip_payload rejects legacy pipe format
- gossip_payload double round-trip stable

---

## Auth — Argon2id

| Executable | File | Cases |
|---|---|---|
| `authentication_test` | `tests/auth/authentication_test.cpp` | 14 |

### Authentication (`[authentication]`)

- Authentication hashPassword encodes Argon2id
- Authentication hashPassword uses a unique salt
- Authentication checkPassword accepts the matching password
- Authentication checkPassword rejects a wrong password
- Authentication empty password hashes and verifies
- Authentication long password hashes and verifies
- Authentication unicode password hashes and verifies
- Authentication whitespace password is significant
- Authentication password with embedded null bytes
- Authentication checkPassword falls back to plaintext compare
- Authentication malformed Argon2 string uses plaintext compare
- Authentication empty stored hash
- Authentication concurrent hash and check
- Authentication typical register login flow

---

## Client

| Executable | File | Cases |
|---|---|---|
| `client_state_test` | `tests/client/client_state_test.cpp` | 10 |
| `packet_builder_test` | `tests/client/packet_builder_test.cpp` | 13 |
| `packet_handler_test` | `tests/client/packet_handler_test.cpp` | 7 |
| `network_test` | `tests/client/network_test.cpp` | 11 |
| `console_ui_test` | `tests/client/console_ui_test.cpp` | 14 |
| `client_test` | `tests/client/client_test.cpp` | 10 |

### ClientState (`[client_state]`)

- ClientState default state is empty
- ClientState isLoggedIn follows user only
- ClientState clear on empty state
- ClientState clear after login session
- ClientState room without user is not logged in
- ClientState user without room is logged in
- ClientState replace user and room
- ClientState clear is idempotent
- ClientState anonymous and empty-field users
- ClientState Lobby room assignment

### PacketBuilder (`[packet_builder]`)

- PacketBuilder buildLogin maps fields
- PacketBuilder buildLogin rejects empty arguments
- PacketBuilder buildRegister maps fields
- PacketBuilder buildRegister rejects empty arguments
- PacketBuilder buildLogout maps fields from User
- PacketBuilder buildLogout rejects empty username or email
- PacketBuilder buildMessage maps fields
- PacketBuilder buildMessage rejects empty arguments
- PacketBuilder buildJoinRoom maps fields
- PacketBuilder buildLeaveRoom maps fields
- PacketBuilder join and leave reject empty arguments
- PacketBuilder sets defaults and timestamp
- PacketBuilder accepts whitespace-only arguments

### PacketHandler (`[packet_handler]`)

- PacketHandler LOGIN success deserializes user
- PacketHandler REGISTER success deserializes user
- PacketHandler rejects non-SUCCESS auth responses
- PacketHandler rejects invalid auth payload
- PacketHandler ignores non-auth packet types
- PacketHandler SUCCESS with empty message fails
- PacketHandler responseCode edge values

### Network (`[network]`)

- Network default is not connected
- Network connect then disconnect
- Network double connect fails
- Network disconnect without connect is a no-op
- Network send while disconnected fails
- Network send and receive round-trip
- Network heartbeat ping receives pong
- Network chat push is not queued
- Network peer close ends receive
- Network send after peer close fails
- Network reconnect after disconnect

### ConsoleUI (`[console_ui]`)

- ConsoleUI showHomeScreen accepts a valid choice
- ConsoleUI showHomeScreen retries after invalid input
- ConsoleUI showHomeScreen EOF returns Exit
- ConsoleUI showUserDashboard without user returns -1
- ConsoleUI showUserDashboard accepts a valid choice
- ConsoleUI showUserDashboard EOF returns Logout
- ConsoleUI showLogin builds a LOGIN packet
- ConsoleUI showLogin cancel on username exit
- ConsoleUI showLogin cancel on password exit
- ConsoleUI showLogin retries empty then succeeds
- ConsoleUI showRegister builds a REGISTER packet
- ConsoleUI showJoinRoom builds a ROOM_JOIN packet
- ConsoleUI showCreateMessage builds a MESSAGE packet
- ConsoleUI showCreateMessage cancel via exit

### Client (`[client]`) — mocked peer, not a live `chat_server`

- Client default is not alive
- Client start fails when nothing is listening
- Client start then stop
- Client double start fails
- Client stop without start is a no-op
- Client home Exit disconnects
- Client login success round-trip
- Client login failure keeps connection
- Client register success round-trip
- Client login then logout

---

## Server

| Executable | File | Cases |
|---|---|---|
| `client_session_test` | `tests/server/client_session_test.cpp` | 11 |
| `database_manager_test` | `tests/server/database_manager_test.cpp` | 18 |
| `heartbeat_test` | `tests/server/heartbeat_test.cpp` | 19 |
| `room_manager_test` | `tests/server/room_manager_test.cpp` | 15 |
| `packet_processor_test` | `tests/server/packet_processor_test.cpp` | 15 |
| `connection_manager_test` | `tests/server/connection_manager_test.cpp` | 18 |
| `gossip_manager_test` | `tests/server/gossip_manager_test.cpp` | 22 |
| `server_test` | `tests/server/server_test.cpp` | 13 |

### ClientSession (`[client_session]`)

- ClientSession constructor defaults
- ClientSession constructor edge sockets
- ClientSession setUser / getUser
- ClientSession setRoom / getRoom
- ClientSession authentication flag
- ClientSession markClosed is one-shot
- ClientSession markClosed concurrent
- ClientSession sendMutex can be locked
- ClientSession heartbeat touch / isAlive
- ClientSession concurrent getters and setters
- ClientSession typical login/logout flow

### DatabaseManager (`[database_manager]`)

- DatabaseManager singleton identity
- DatabaseManager seed users
- DatabaseManager getUser edges
- DatabaseManager userExists edges
- DatabaseManager createUser success
- DatabaseManager createUser constraints
- DatabaseManager createUser string edges
- DatabaseManager loginUser success
- DatabaseManager loginUser failures
- DatabaseManager saveMessage success and duplicate id
- DatabaseManager saveMessage foreign keys
- DatabaseManager saveMessage content edges
- DatabaseManager loadHistory order and reconstruct
- DatabaseManager loadHistory limit
- DatabaseManager online presence
- DatabaseManager membership edges
- DatabaseManager concurrent writes
- DatabaseManager typical register login logout flow

### Heartbeat (`[heartbeat]`)

- Heartbeat destructor without start does not log Stopped
- Heartbeat stop without start is a no-op
- Heartbeat start then stop logs Started and Stopped
- Heartbeat double start logs Started once
- Heartbeat double stop logs Stopped once
- Heartbeat can restart after stop
- Heartbeat stop returns before the next interval
- Heartbeat does not ping before the first interval
- Heartbeat destructor stops a running worker
- Heartbeat concurrent start starts once
- Heartbeat concurrent stop stops once
- Heartbeat concurrent start and stop do not deadlock
- Heartbeat many start stop cycles stay consistent
- Heartbeat empty snapshot logs total 0
- Heartbeat live client receives ping
- Heartbeat skips closed sessions
- Heartbeat closes a silent client after timeout
- Heartbeat pings every live client
- Heartbeat typical start ping stop flow

### RoomManager (`[room_manager]`)

- RoomManager singleton identity
- RoomManager getRoom edges
- RoomManager createRoom edges
- RoomManager deleteRoom refuses Lobby and unknown
- RoomManager joinRoom unknown and empty name
- RoomManager joinRoom moves between rooms
- RoomManager joinRoom same room is idempotent
- RoomManager leaveAll removes membership without Lobby insert
- RoomManager authenticated join persists and leaveAll clears
- RoomManager broadcast empty room succeeds
- RoomManager broadcast delivers and respects skipSocket
- RoomManager broadcastAll fans out across rooms
- RoomManager deleteRoom moves members to Lobby
- RoomManager concurrent joins are safe
- RoomManager typical create join leave delete flow

### PacketProcessor (`[packet_processor]`)

- PacketProcessor heartbeat returns pong
- PacketProcessor rejects gossip on client port
- PacketProcessor rejects unknown packet type
- PacketProcessor register rejects missing fields
- PacketProcessor register success and duplicate
- PacketProcessor login failures
- PacketProcessor login success
- PacketProcessor login rejects already online
- PacketProcessor logout
- PacketProcessor message auth and empty
- PacketProcessor message success with pipes
- PacketProcessor room join leave edges
- PacketProcessor room join and leave
- PacketProcessor response envelope
- PacketProcessor typical register message logout flow

### ConnectionManager (`[connection_manager]`)

- ConnectionManager destructor without listen is quiet
- ConnectionManager stop without start warns
- ConnectionManager start then stop
- ConnectionManager double start refuses
- ConnectionManager double stop warns once then again
- ConnectionManager restart after stop
- ConnectionManager accept creates anonymous Lobby session
- ConnectionManager client ping receives pong
- ConnectionManager ignores non-ping heartbeat
- ConnectionManager sendPacket edges
- ConnectionManager closeClient removes session
- ConnectionManager hasSession after login
- ConnectionManager rumor with and without gossip
- ConnectionManager disconnect clears online without gossip
- ConnectionManager disconnect rumored logout with gossip
- ConnectionManager accepts multiple clients
- ConnectionManager stopListening closes clients
- ConnectionManager typical connect login message disconnect flow

### GossipManager (`[gossip_manager]`)

- GossipManager destructor without start does not log Stopped
- GossipManager stop without start is a no-op
- GossipManager start then stop logs Listening and Stopped
- GossipManager double start listens once
- GossipManager double stop logs Stopped once
- GossipManager restart after stop listens again
- GossipManager rumor empty event id is a no-op
- GossipManager rumor LOGIN and LOGOUT update presence
- GossipManager rumor USER_CREATED inserts user
- GossipManager rumor ROOM_JOIN and ROOM_LEAVE
- GossipManager rumor MESSAGE saves history
- GossipManager duplicate rumor event id is ignored
- GossipManager rumor malformed payload does not crash
- GossipManager peer HELLO registers remote node
- GossipManager peer HELLO rejects self and duplicate node id
- GossipManager ignores EVENT before HELLO
- GossipManager applies EVENT after HELLO
- GossipManager DIGEST pull fills missing events
- GossipManager dial bad address logs warning
- GossipManager dial connects to listening peer
- GossipManager concurrent rumor is safe
- GossipManager typical LOGIN MESSAGE LOGOUT flow

### Server (`[server]`)

- Server destructor without start does not start listening
- Server stop without start is a no-op
- Server start then stop logs Started and Stopped
- Server double start logs Started once
- Server double stop logs Stopped once
- Server can restart after stop
- Server destructor stops a running server
- Server dashboard while stopped
- Server dashboard while running
- Server dashboard lists peers
- Server start fails when the client port is exclusive
- Server accept loop accepts a client
- Server typical start dashboard stop flow

---

## Functionality still untested

These are product behaviors that exist (or are TODOs) and do not yet have a Catch2 case. Highest value first.

### End-to-end (live `chat_server` + `chat_client` / Client)

- Two clients on one node: login, join the same room, send a MESSAGE; the other client receives the push (Network logs it, does not queue it).
- Client join-room round-trip: dashboard choice 2 updates `ClientState.currentRoom`.
- Client leave-room round-trip: dashboard choice 3 returns to Lobby; refuse leave while already in Lobby.
- Client send-message round-trip: dashboard choice 4, SUCCESS vs ERROR.
- Client cannot leave Lobby; join unknown room returns NOT_FOUND and state is unchanged.
- Disconnect / Ctrl+C while logged in: server clears `online_users` and rumors LOGOUT.
- Client reconnect after server restart; session is anonymous until login again.
- Register then login on a second client with the same username is rejected (`user already logged in`).
- Logout then login again on the same connection.
- Update profile (dashboard choice 1) — still a TODO in `Client::showDashboard`.

### Cluster / gossip (two nodes)

- Two `Server` processes with `--peers`: HELLO both ways, then a LOGIN on node A appears in node B `online_users`.
- USER_CREATED on A: login on B with the same credentials succeeds (password/email replicated).
- Duplicate login across nodes: A holds the socket, B rejects LOGIN.
- ROOM_JOIN on A updates membership on B; ROOM_LEAVE / return to Lobby replicates.
- MESSAGE on A is persisted on B and broadcast to B’s local room sockets.
- Anti-entropy: B is down while A rumored events; after B starts, DIGEST/PULL fills the gap.
- Event-log cap (`MAX_EVENT_LOG` 256): oldest ids dropped; late PULL cannot resurrect them.
- Peer disconnect and re-dial (`DIAL_INTERVAL`); duplicate NODE_ID rejected.
- Gossip packets on the client port stay rejected (already unit-tested) while the peer port accepts them.

### Persistence and rooms

- `loadHistory` after join: client never requests history today; if added, test newest-first reconstruct.
- Server restart with the same `--db`: users, rooms, messages, membership survive; `online_users` should be empty until login.
- `createRoom` / `deleteRoom` over the wire (no client packet type yet; RoomManager only).
- Private rooms vs PUBLIC; room types other than Lobby (schema supports them, client always joins by name).
- ADMIN vs USER vs GUEST privileges (seed has ADMIN; no permission checks yet).
- Unique email constraint on `createUser` (username uniqueness is covered).

### Heartbeat (client + server together)

- Live `Network::readerLoop` pongs `Heartbeat` pings so the session is not timed out.
- Silent client (no pong) is closed after `HEARTBEAT_TIMEOUT`; `online_users` cleared.
- Heartbeat ping is not delivered to `Client::waitFor`.

### Missing unit / config surfaces

- **ThreadPool**: enqueue, worker execution, shutdown while tasks queued, destructor, concurrent `task()`.
- **config::parseArgs / parsePort / parsePeers**: `--host`, `--port`, `--peer-port`, `--peers`, `--db`, `--node-id`, `--help`, invalid port, unknown flag, missing value.
- **PacketHandler** only handles LOGIN/REGISTER; ROOM_JOIN / ROOM_LEAVE / MESSAGE / LOGOUT success payloads are ignored (Client inspects `responseCode` instead). Worth a contract test so that does not regress.
- ConsoleUI has no leave-room prompt (leave is dashboard choice 3 with no extra input) — cover cancel/EOF on `showRegister` / `showJoinRoom` the same way as login.
- `waitFor` skipping unexpected queued types (non-heartbeat packets of the wrong type).
- `Network` receive timeout / blocking `receivePacket` with no packet.
- `socket_io::listenTo` / `connectTo` failure (port in use, bad host) as named cases if not already asserted inside edges.

### Concurrency / robustness

- Two clients sending MESSAGE in the same room at once; history order and broadcast to both.
- Login + gossip rumor + heartbeat ping overlapping on one session.
- Rapid connect/disconnect of many sockets (`THREAD_COUNT` workers).
- Malformed / oversized client frames do not crash `handleClient`.
- SQLite lock timeout under gossip + packet_processor + heartbeat concurrent DB writes (partially covered by `DatabaseManager concurrent writes`).
