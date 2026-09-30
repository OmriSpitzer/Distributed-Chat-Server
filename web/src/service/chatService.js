/**
 * Chat service
 * @brief Socket, shared session, and live transcript.
 * @date 2026-09-29
 *
 * User, room, and admin actions live in their own services and share this session.
 */

import {
  encodePacket, encodePong, decodePacket, parseHistory, parseRoomList, parseUser, PacketType,
} from "./packet.js";
import { isAdmin } from "./adminService.js";
import { isLoggedIn } from "./userService.js";

const SOCKET_URL = import.meta.env.VITE_SERVER_URL ?? "ws://127.0.0.1:8080";

function connectedLabel() {
  try {
    const parsed = new URL(SOCKET_URL);
    const port = parsed.port || (parsed.protocol === "wss:" ? "443" : "80");
    return `Connected ${parsed.hostname}:${port}`;
  } catch {
    return "Connected";
  }
}
const LISTENERS = new Set();
const WAITERS = [];
let CONN_SOCKET = null;
let HOLD_CHAT = false;
let PENDING_CHAT = [];
let TRANSCRIPT = 0;

export const session = {
  link: "Disconnected",
  busy: false,
  user: null,
  rooms: [],
  currentRoom: null,
  transcript: [],
};

export function lobbyRoom() {
  return { id: 1, name: "Lobby", type: "LOBBY", privacy: "PUBLIC" };
}

session.currentRoom = lobbyRoom();

export function anonymousUser() {
  const suffix = String(Math.floor(Math.random() * 10000)).padStart(7, "0");
  const username = `anon${suffix}`;
  return { username, email: `${username}@local`, type: "GUEST" };
}

export function connected() {
  return CONN_SOCKET?.readyState === WebSocket.OPEN;
}

function snapshot() {
  const current = session.currentRoom?.name ?? "Lobby";
  const rooms = [...session.rooms].sort((a, b) => a.id - b.id);
  return {
    link: session.link,
    busy: session.busy,
    connected: session.link.startsWith("Connected"),
    loggedIn: isLoggedIn(),
    admin: isAdmin(),
    username: session.user?.username ?? "Guest",
    email: session.user?.email ?? "",
    rooms: rooms.map((room) => ({
      id: room.id,
      name: room.name,
      privacy: room.privacy === "PRIVATE" ? "Private" : "Public",
      here: room.name === current,
    })),
    waiting: rooms.length === 0 && session.link.startsWith("Connected"),
    currentRoom: current,
    inLobby: current === "Lobby",
    transcript: session.transcript.map((line) => ({ ...line })),
  };
}

export function notify() {
  const current = snapshot();
  for (const listener of LISTENERS) {
    listener(current);
  }
}

export function roomFromName(name) {
  if (name === "Lobby") {
    return lobbyRoom();
  }
  const known = session.rooms.find((room) => room.name === name);
  if (known) {
    return { ...known };
  }
  return { id: 0, name, type: "OTHER", privacy: "PUBLIC" };
}

export function applyRoomDirectory(encoded) {
  session.rooms = parseRoomList(encoded);
}

export function rememberRoom(name) {
  if (!name || session.rooms.some((room) => room.name === name)) {
    return;
  }
  session.rooms.push({ id: 0, name, type: "OTHER", privacy: "PUBLIC" });
}

function applyRoomList(packet) {
  if (packet.message.startsWith("room(")) {
    applyRoomDirectory(packet.message);
  } else if (packet.room.startsWith("room(")) {
    applyRoomDirectory(packet.room);
  }

  if (packet.room && !packet.room.startsWith("room(")) {
    session.currentRoom = roomFromName(packet.room);
  }

  const userPayload = packet.receiver.startsWith("user(") ? packet.receiver : packet.sender;
  if (userPayload.startsWith("user(")) {
    const user = parseUser(userPayload);
    if (user) {
      session.user = user;
      session.transcript = [];
      PENDING_CHAT = [];
    }
  }
}

function enqueueChat(packet) {
  if (session.currentRoom && packet.room && packet.room !== session.currentRoom.name) {
    return;
  }
  const line = {
    author: packet.sender,
    text: packet.message,
    timestamp: packet.timestamp || Math.floor(Date.now() / 1000),
  };
  if (HOLD_CHAT) {
    PENDING_CHAT.push(line);
    return;
  }
  session.transcript.push(line);
  notify();
}

function finishWaiter(waiter, packet) {
  const index = WAITERS.indexOf(waiter);
  if (index !== -1) {
    WAITERS.splice(index, 1);
  }
  clearTimeout(waiter.timer);
  waiter.resolve(packet);
}

function failWaiters() {
  for (const waiter of [...WAITERS]) {
    finishWaiter(waiter, null);
  }
}

function resumeSignedIn() {
  if (!session.user || session.user.type === "GUEST") {
    return;
  }
  const room = session.currentRoom?.name || "Lobby";
  request({
    type: PacketType.RECONNECT,
    sender: session.user.username,
    room,
  }).then((response) => {
    if (!response || response.responseCode !== 200) {
      return;
    }
    const user = parseUser(response.message);
    if (user) {
      session.user = user;
    }
    if (response.room) {
      applyRoomDirectory(response.room);
    }
    notify();
  });
}

function onFrame(event) {
  const packet = decodePacket(event.data);
  if (!packet || !connected()) {
    return;
  }
  if (packet.type === PacketType.HEARTBEAT && packet.message === "ping") {
    CONN_SOCKET.send(encodePong());
    return;
  }
  // Pushes use 0. Replies use 200 on success.
  if (packet.responseCode === 0) {
    if (packet.type === PacketType.ROOM_LIST) {
      applyRoomList(packet);
      notify();
    } else if (packet.type === PacketType.MESSAGE) {
      enqueueChat(packet);
    }
    return;
  }
  const waiter = WAITERS.find((item) => item.type === packet.type);
  if (waiter) {
    finishWaiter(waiter, packet);
  }
}

function openSocket() {
  if (CONN_SOCKET && (CONN_SOCKET.readyState === WebSocket.OPEN || CONN_SOCKET.readyState === WebSocket.CONNECTING)) {
    return;
  }
  const current = new WebSocket(SOCKET_URL);
  CONN_SOCKET = current;
  current.binaryType = "arraybuffer";
  current.onopen = () => {
    if (CONN_SOCKET !== current) {
      return;
    }
    session.link = connectedLabel();
    notify();
    resumeSignedIn();
  };
  current.onmessage = onFrame;
  current.onerror = () => {
    if (CONN_SOCKET !== current) {
      return;
    }
    session.link = "Disconnected";
    notify();
  };
  current.onclose = () => {
    if (CONN_SOCKET !== current) {
      return;
    }
    session.link = "Disconnected";
    failWaiters();
    notify();
    if (LISTENERS.size > 0) {
      setTimeout(openSocket, 1000);
    }
  };
}

export function request(packet) {
  if (!connected()) {
    return Promise.resolve(null);
  }
  return new Promise((resolve) => {
    const waiter = { type: packet.type, resolve };
    waiter.timer = setTimeout(() => finishWaiter(waiter, null), 15000);
    WAITERS.push(waiter);
    CONN_SOCKET.send(encodePacket(packet));
  });
}

export function withBusy(fn) {
  if (session.busy) {
    return Promise.resolve("busy");
  }
  session.busy = true;
  notify();
  return Promise.resolve()
    .then(fn)
    .finally(() => {
      session.busy = false;
      notify();
    });
}

// "" when the reply is 200. Otherwise the text to show.
export function refused(response, lost, failed) {
  if (!response) {
    return lost;
  }
  if (response.responseCode !== 200) {
    return response.message || failed;
  }
  return "";
}

export async function resetTranscript() {
  const gen = ++TRANSCRIPT;
  HOLD_CHAT = true;
  PENDING_CHAT = [];
  session.transcript = [];
  notify();

  let error;
  if (!connected() || !session.user || !session.currentRoom) {
    error = "No room to load history for.";
  } else {
    const response = await request({
      type: PacketType.LOAD_MESSAGE_HISTORY,
      sender: session.user.username,
      room: session.currentRoom.name,
    });
    if (gen !== TRANSCRIPT) {
      return "";
    }
    error = refused(response, "Connection lost while loading history.", "Load history failed.");
    if (!error) {
      session.transcript = parseHistory(response.message);
    }
  }

  if (gen !== TRANSCRIPT) {
    return "";
  }
  if (error) {
    session.transcript = [{ author: "system", text: error, timestamp: 0 }];
  }
  session.transcript.push(...PENDING_CHAT);
  PENDING_CHAT = [];
  HOLD_CHAT = false;
  notify();
  return error;
}

export function watchState(listener) {
  LISTENERS.add(listener);
  openSocket();
  listener(snapshot());
  return () => {
    LISTENERS.delete(listener);
    if (LISTENERS.size === 0 && CONN_SOCKET) {
      CONN_SOCKET.close();
      CONN_SOCKET = null;
    }
  };
}
