# Data (`src/data`)

Sample / local copy of the pipe-delimited database. The server singleton uses `config::DB_PATH` (`data/db.txt` relative to the process working directory), which usually means **`data/db.txt` at the repo root**, not this folder.

Keep the same three-section layout in both files if you copy them.

## Files

### `db.txt`

Empty template:

```
[USERS]
[ROOMS]
[MESSAGES]
```

`DatabaseManager` creates this shape if the configured path is missing.

## Row formats

Created and read in `src/server/database_manager.cpp`.

| Section | Line |
|---------|------|
| `[USERS]` | `username\|email\|userType\|password` |
| `[ROOMS]` | `id\|name\|type\|privacy` |
| `[MESSAGES]` | `id\|roomId\|fromUsername\|toUsername\|content\|timestamp` |

`userType` is the `User::UserType` integer (`0` ADMIN, `1` USER, else GUEST). Room `type` and `privacy` are the corresponding enum integers. Passwords are stored in plain text.

Blank lines are skipped. Section headers switch which list a following line belongs to.

## Related

- [../server/SERVER.md](../server/SERVER.md) — `DatabaseManager`
- [../auth/AUTH.md](../auth/AUTH.md) — login uses `getPassword` / `userExists`
