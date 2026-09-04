# Client (`src/client`)

Console client: welcome menu, packet construction, response handlers, and a network facade. Built as `client_lib` plus executable `chat_client`.

Headers: `include/client/`. Several UI and network methods are stubs; TCP is not implemented.

## Files

### `main.cpp`

Creates `Client`, calls `start()` then `stop()`.

### `client.cpp` / `include/client/client.h`

Facade owning `Network`, `ConsoleUI`, `PacketHandler`, and `PacketBuilder`. `start()` assigns an incrementing local `id`, calls `network.connect()`, then loops `ConsoleUI::showWelcome()` until the user chooses Exit (3). Login (1) and Register (2) cases are empty. `stop()` calls `network.disconnect()`.

`CommandParser` is commented out in the header.

### `network.cpp` / `include/client/network.h`

Transport stub (no sockets):

- `connect()` — prints and returns `true`.
- `disconnect()` — prints.
- `sendPacket` — prints sender, returns `true`.
- `receivePacket` — returns a dummy `MESSAGE` from `"server"`.

### `console_ui.cpp` / `include/client/console_ui.h`

Stdout UI.

- `showWelcome()` — menu: 1 Login, 2 Register, 3 Exit; returns the choice (header currently declares `void`; implementation returns `int`).
- `showLogin()`, `showRooms()`, `printMessage()` — placeholder text.
- `clearScreen()` — `cls` on Windows, `clear` elsewhere.
- `showError()` is declared; no `.cpp` definition.

### `packet_builder.cpp` / `include/client/packet_builder.h`

Static helpers that fill a `Packet`:

| Method | Type | Fields |
|--------|------|--------|
| `buildLogin(username, password)` | `LOGIN` | `sender`, `message` = password, timestamp |
| `buildMessage(msg)` | `MESSAGE` | from/to usernames, content, message timestamp |
| `buildJoinRoom(room)` | `ROOM_JOIN` | `room`, timestamp |

Not yet called from `Client::start()`.

### `packet_handler.cpp` / `include/client/packet_handler.h`

Prints server-side packets: chat line, login response, room list, error. No dispatch-from-`Network` loop.

## Headers without implementations in this folder

### `include/client/client_state.h`

Holds `User`, current `Room`, `connected`, and `loggedIn`. Not used by `Client` yet.

### `include/client/command_parser.h`

Empty placeholder. Intended for slash-commands or similar.

## Related

- Packet types: [../utils/models/MODELS.md](../utils/models/MODELS.md)
- Server that will accept these packets: [../server/SERVER.md](../server/SERVER.md)
