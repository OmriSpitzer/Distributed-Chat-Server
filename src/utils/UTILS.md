# Utils (`src/utils`)

Shared domain types and logging, built as the `utils` library. Both `server_lib` and `client_lib` link it.

There is no code directly in `src/utils/`; everything is under `models/`.

## Layout

```
src/utils/
└── models/          Implementations  →  MODELS.md
include/utils/models/   Headers (plus packet.h, header-only)
```

## Library sources (`CMakeLists.txt`)

- `logger.cpp`
- `log_message.cpp`
- `message.cpp`
- `user.cpp`
- `room.cpp`

`Packet` is defined only in `include/utils/models/packet.h` (no `.cpp`).

## Tests

Catch2 binaries in `tests/class/`: `logger_test`, `log_message_test`, `user_test`, `message_test`, `room_test`.

See [models/MODELS.md](models/MODELS.md) for each type.
