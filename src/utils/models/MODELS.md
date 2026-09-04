# Models (`src/utils/models`)

Value types shared by client and server. Headers in `include/utils/models/`.

## Files

### `user.cpp` / `user.h`

`User`: `username`, `email`, `UserType` (`ADMIN`, `USER`, `GUEST`). Getters/setters, `operator<<`, equality **by email**.

### `message.cpp` / `message.h`

Chat message: references to `from` and `to` `User`, `content`, `timestamp`, auto-increment `id`. Comparable by timestamp; equality is id + timestamp. Callers must keep the `User` objects alive (see `DatabaseManager::userPool`).

### `room.cpp` / `room.h`

Chat room: generated `id`, `name`, department-style `RoomType`, `Privacy` (`PUBLIC` / `PRIVATE`), user map (keyed by email), message history, capacity 20.

- `addUser` / `addMessage` / `removeUser` / `getUser` / `isEmpty` / `ping`
- Authorization parameters on some methods are unused (`TODO`)
- `ping()` is “not empty”, not a network check

### `logger.cpp` / `logger.h`

Process-wide singleton. `logInfo` / `logWarning` / `logError` enqueue a `LogMessage` and print it. `getMessage(index)` walks a copy of the queue. `operator<<` dumps all entries.

### `log_message.cpp` / `log_message.h`

One log line: id, source, text, `Type` (`INFO`, `WARNING`, `ERROR`), timestamp. Equality by id.

### `include/utils/models/packet.h` (header-only)

Wire-oriented struct (not in the `utils` source list):

| Field | Role |
|-------|------|
| `type` | `LOGIN`, `LOGOUT`, `MESSAGE`, `ROOM_JOIN`, `ROOM_LEAVE` |
| `sender` / `receiver` | Usernames |
| `room` | Room id/name |
| `message` | Payload (password on login, chat text, status) |
| `timestamp` | Unix time |

Client builders and `PacketProcessor` share this type. There is no serialize/deserialize yet.

## Related

- [../UTILS.md](../UTILS.md)
- Server routing: [../../server/SERVER.md](../../server/SERVER.md)
- Client builders: [../../client/CLIENT.md](../../client/CLIENT.md)
