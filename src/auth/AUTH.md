# Auth (`src/auth`)

Username/password checks against `DatabaseManager`. Compiled into `server_lib` (not a separate CMake library).

Header: `include/auth/authentication.h`.

## Files

### `authentication.cpp`

All methods are static.

| Method | Behavior |
|--------|----------|
| `registerUser(username, password)` | Builds `User(username, username@chat.local, USER)` and `createUser` with password. Throws if the name exists. |
| `login(username, password)` | `verifyPassword`, then `findUser`. Throws on bad credentials. |
| `logout(username)` | Prints to stdout. |
| `verifyPassword(username, password)` | False if user missing; else compares stored password (plain text). |

Used by `PacketProcessor` for `LOGIN` / `LOGOUT`. Register is not exposed on a packet type yet.

## Related

- [../server/SERVER.md](../server/SERVER.md) — `PacketProcessor`, `DatabaseManager`
- [../data/DATA.md](../data/DATA.md) — user row format
