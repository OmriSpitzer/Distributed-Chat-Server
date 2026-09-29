# Browser client

[![React](https://img.shields.io/badge/React-19-149ECA?logo=react&logoColor=white)](https://react.dev/) [![Vite](https://img.shields.io/badge/Vite-8-646CFF?logo=vite&logoColor=white)](https://vite.dev/) [![Tailwind CSS](https://img.shields.io/badge/Tailwind-4-06B6D4?logo=tailwindcss&logoColor=white)](https://tailwindcss.com/) [![WebSocket](https://img.shields.io/badge/transport-WebSocket-010101)](../README.md#browser-websocket)

Same chat dashboard as the Qt client, in the browser. It opens one WebSocket to `chat_server` and sends the existing length-prefixed `Packet` frames. There is no REST API, no cookie, and no JWT.

```
  browser ──ws://host:8080──►  chat_server ──gossip──►  peer nodes
                                    │
                                    ▼
                               SQLite (that node)
```

Passwords ride inside a `Packet` and are hashed with Argon2id on the node. The page does not write them to `localStorage` or `sessionStorage`.

**Read next:** [../README.md](../README.md) (wire format, server flags) · [../architecture.md](../architecture.md) (`WebConnection`) · [../FUTURE_WORK.md](../FUTURE_WORK.md) (§4 website)

## Quick start

Node.js `^20.19.0` or `>=22.12.0` (what Vite 8 requires). Start a server from the repo root so `--ws-port` is listening, then start Vite from this folder.

```powershell
# repo root — default WebSocket port is 8080
.\scripts\run_server.ps1

# this folder
npm install
npm run dev
```

Open the local URL Vite prints (usually `http://localhost:5173`). The header turns green when the socket is up.

Seed accounts from the node database: `admin` / `admin` (ADMIN) and `user` / `user`. Guests can join public rooms and send messages before they sign in; the server assigns an anonymous user on the welcome push.

Point the socket somewhere else with `VITE_SERVER_URL` (a `.env` file in this folder is gitignored):

```
VITE_SERVER_URL=ws://127.0.0.1:8080
```

For a public site, terminate TLS at the reverse proxy (`wss://…`) and keep the chat port private. The app still sends binary frames; the proxy only wraps the socket.

## What you can do

| Action | Who | Notes |
|--------|-----|--------|
| Sign in / Log in / Logout | guest / signed-in | Sign in is register. One live session per user across the cluster. |
| Update profile | signed-in | Username and optional new password. Email is shown and not edited. |
| Join / Leave | anyone with a welcome user | Double-click a room, or use Join. Leave returns to Lobby. Lobby itself cannot be left. |
| Create room | signed-in | Public or private. The page joins the new room after create succeeds. |
| Invite | signed-in | Allow-lists a username for the current room. Not available from Lobby. |
| Kick / Delete room | ADMIN | Delete refuses Lobby and General. |
| Send / history | anyone in a room | History loads on join. Live pushes append while you stay in that room. |

A heartbeat `ping` is answered with `pong` and is not shown in the transcript. If the socket drops, the page retries about one second later while it stays open.

## How a click becomes a packet

`App.jsx` holds the dashboard state and the click handlers. The panels under `src/assets/` only render. `chatService.js` owns the socket and the shared session. `userService.js`, `roomService.js`, and `adminService.js` send the requests. `packet.js` matches `Packet::PacketType` and the C++ frame layout.

```
App  →  user / room / admin service  →  chatService  →  WebSocket binary frame  →  WebConnection  →  PacketProcessor
```

Frame layout (big-endian), max payload enforced on the server:

```
[u32 size][u8 type][u64 timestamp][u32 responseCode]
  then sender, receiver, room, message as [u32 length][bytes]
```

Requests wait up to 15 seconds for a reply with the same type. Pushes use `responseCode == 0` (`ROOM_LIST`, live `MESSAGE`) and do not complete that wait. A service returns `""` when that code is `200`, and the server’s error text otherwise.

`binaryType` is `arraybuffer`. Each WebSocket message is one framed packet, the same bytes a Qt client would write on TCP.

## Scripts

| Command | What it does |
|---------|----------------|
| `npm run dev` | Vite dev server with hot reload |
| `npm run build` | Production bundle in `dist/` |
| `npm run preview` | Serve the production bundle locally |
| `npm run lint` | ESLint on `src/` |

## Layout

```
web/
├── index.html
├── vite.config.js          React, Tailwind 4, React Compiler
├── eslint.config.js
├── package.json
└── src/
    ├── main.jsx
    ├── App.jsx             Session state and click handlers
    ├── index.css           Tailwind import
    ├── assets/
    │   ├── Header.jsx      Connection status and account actions
    │   ├── RoomSidebar.jsx Room list and room actions
    │   ├── ChatPanel.jsx   Transcript and composer
    │   ├── FormDialog.jsx  Shared input dialog
    │   └── AlertDialog.jsx Success and error popup
    └── service/
        ├── chatService.js  Socket, session, and live transcript
        ├── userService.js  Sign-in, profile, and signed-in checks
        ├── roomService.js  Join, leave, create, invite, and send
        ├── adminService.js Kick, delete, and the admin check
        └── packet.js       Encode / decode and payload parsers
```

Tailwind is loaded by the Vite plugin (`@tailwindcss/vite`). There is no separate `tailwind.config` file.
