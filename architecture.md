# Architecture

C++17 distributed chat: `chat_server` and `chat_client` are separate processes. They meet only on TCP as framed `Packet`s (`socket_io` + `Serializer`). Cluster nodes also gossip on a peer port. Each binary defaults to a Qt Widgets dashboard; `--test` runs the console UI.

Shared types (`Packet`, `User`, `Room`, `Message`, `Logger`, `gossip_payload`) live in `utils`. They are not a third process. Qt stays in the executables: widgets read `Server*` / `Client*` on the GUI thread and do not live in `server_lib` / `client_lib` domain or transport code.

---

## Layered architecture

Dependencies go downward. Upper layers call lower ones; lower layers do not depend on UI or `PacketProcessor` / `Client`.

### Server

```mermaid
flowchart TB
  subgraph S_L0["0. Presentation (executable)"]
    direction LR
    MainWindow
    PortsPanel
    UsersPanel
    RoomsPanel
    LogPanel
  end

  subgraph S_L1["1. Transport"]
    direction LR
    ConnectionManager
    Heartbeat
    GossipManager
    socket_io
    Serializer
  end

  subgraph S_L2["2. Application"]
    direction LR
    Server
    PacketProcessor
    ClientSession
    ThreadPool
  end

  subgraph S_L3["3. Domain"]
    direction LR
    RoomManager
    Authentication
    Packet
    User
    Room
    Message
  end

  subgraph S_L4["4. Persistence"]
    direction LR
    DatabaseManager
    gossip_payload
    SQLite[("SQLite")]
  end

  S_L0 --> S_L2
  S_L1 --> S_L2
  S_L2 --> S_L3
  S_L3 --> S_L4
```

| Layer | Classes | Role |
|-------|---------|------|
| Presentation | `MainWindow`, `PortsPanel`, `UsersPanel`, `RoomsPanel`, `LogPanel` | Qt dashboard; timer refresh from `config`, sessions, rooms, `Logger` |
| Transport | `ConnectionManager`, `Heartbeat`, `GossipManager`, `socket_io`, `Serializer` | Accept clients, ping/drop sockets, rumor to peers, frame bytes |
| Application | `Server`, `PacketProcessor`, `ClientSession`, `ThreadPool` | Lifecycle, route each packet, per-socket auth/room state |
| Domain | `RoomManager`, `Authentication`, `Packet`, `User`, `Room`, `Message` | Membership, broadcast rules, password hash/verify, wire types |
| Persistence | `DatabaseManager`, `gossip_payload`, SQLite | Users, online presence, membership, message history, event bodies |

### Client

```mermaid
flowchart TB
  subgraph C_L1["1. Presentation"]
    direction LR
    DashboardPage
    ConsoleUI
  end

  subgraph C_L2["2. Application"]
    direction LR
    Client
    PacketBuilder
    PacketHandler
    ClientState
  end

  subgraph C_L3["3. Transport"]
    direction LR
    Network
    socket_io
    Serializer
  end

  subgraph C_L4["4. Shared domain"]
    direction LR
    Packet
    User
    Room
  end

  C_L1 --> C_L2
  C_L2 --> C_L3
  C_L3 --> C_L4
```

| Layer | Classes | Role |
|-------|---------|------|
| Presentation | `DashboardPage`, `ConsoleUI` | Qt dashboard + dialogs, or console menus (`--test`) |
| Application | `Client`, `PacketBuilder`, `PacketHandler`, `ClientState` | Action methods (`joinRoom`, `login`, …), parse replies, remember user/room |
| Transport | `Network`, `socket_io`, `Serializer` | Connect, send, reader thread (pong heartbeats, `ROOM_LIST` / `MESSAGE` pushes) |
| Shared domain | `Packet`, `User`, `Room` | Same models as the server wire |

---

## Server class interactions

```mermaid
flowchart TB
  Server["Server"]

  subgraph OWNED["Owned by Server"]
    direction LR
    ThreadPool
    Heartbeat
    GossipManager
    ConnectionManager
  end

  Server --> ThreadPool
  Server --> Heartbeat
  Server --> GossipManager
  Server -->|"startListening / acceptLoop"| ConnectionManager

  Heartbeat -->|"HEARTBEAT ping, close stale"| ConnectionManager
  ConnectionManager -->|"setGossip / rumor"| GossipManager
  ConnectionManager -->|"accept"| ClientSession
  ConnectionManager -->|"handleClient → processPacket"| PacketProcessor

  PacketProcessor -->|"loginUser / createUser / isUserOnline"| DatabaseManager
  PacketProcessor -->|"joinRoom / leaveAll"| RoomManager
  PacketProcessor -->|"setUser / setAuthenticated"| ClientSession
  PacketProcessor -->|"rumor LOGIN, USER_CREATED, ROOM_*, MESSAGE"| ConnectionManager

  RoomManager -->|"setMembership / clearMembership"| DatabaseManager
  RoomManager -->|"broadcast sendPacket"| ConnectionManager
  GossipManager -->|"applyEvent"| DatabaseManager
  GossipManager -->|"MESSAGE inserted → broadcast"| RoomManager
  GossipManager -->|"GOSSIP_HELLO / EVENT / DIGEST / PULL"| Peers["Peer servers :PEER_PORT"]
  DatabaseManager -->|"hashPassword / checkPassword"| Authentication
  DatabaseManager --> SQLite[("SQLite")]
```

---

## Client class interactions

```mermaid
flowchart TB
  Client["Client"]

  Client -->|"showDashboard_2"| DashboardPage
  Client -->|"showDashboard"| ConsoleUI
  Client -->|"connect / sendPacket / waitFor"| Network
  Client -->|"handlePacket"| PacketHandler
  Client --> ClientState

  DashboardPage -->|"joinRoom / leaveRoom / login / sendMessage / …"| Client
  ConsoleUI -->|"prompts → Client actions"| Client
  Client -->|"PacketBuilder"| PacketBuilder
  PacketHandler -->|"deserialize User on LOGIN / REGISTER"| ClientState
  Network -->|"readerLoop: pong HEARTBEAT, ROOM_LIST, enqueue MESSAGE, queue replies"| Network
```

`Network` is the only client class that talks to the server. Widgets and `ConsoleUI` never touch a socket; they call `Client` on the UI / main thread. `MESSAGE` pushes are queued under a mutex and drained on the Qt timer (not from the reader thread).

---

## Across the wire

Processes stay separate. Only `Network` and `ConnectionManager` share a TCP connection.

```mermaid
sequenceDiagram
  autonumber
  box rgb(230,240,255) Client
    participant UI as DashboardPage / ConsoleUI
    participant C as Client
    participant PB as PacketBuilder
    participant N as Network
  end
  box rgb(255,240,230) Server
    participant CM as ConnectionManager
    participant CS as ClientSession
    participant PP as PacketProcessor
    participant DB as DatabaseManager
    participant RM as RoomManager
    participant GM as GossipManager
  end

  UI->>C: joinRoom / login / sendMessage / …
  C->>PB: build LOGIN / REGISTER / ROOM_* / MESSAGE / UPDATE_USER
  PB-->>C: Packet
  C->>N: sendPacket
  N->>CM: TCP writePacket
  CM->>CS: touch session
  CM->>PP: processPacket
  PP->>DB: loginUser / createUser / membership
  PP->>RM: joinRoom / leaveAll
  PP->>CM: rumor GOSSIP_EVENT
  CM->>GM: rumor → applyEvent + fan-out to peers
  GM->>RM: broadcast MESSAGE to room sockets
  PP-->>CM: response Packet
  CM->>N: TCP sendPacket
  N->>C: waitFor expected type
  C->>C: PacketHandler + ClientState
```

Heartbeat is out of band: `Heartbeat` pings every session; `Network::readerLoop` replies `pong` and does not queue those packets for `Client`. Room directory and chat pushes use `responseCode == 0` and the push handler, not `waitFor`.
