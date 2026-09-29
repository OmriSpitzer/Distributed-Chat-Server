import {
  encodePacket,
  encodePong,
  decodePacket,
  parseHistory,
  parseRoomList,
  parseUser,
  PacketType,
  ResponseCode,
} from "./packet.js";

const socketUrl = import.meta.env.VITE_SERVER_URL ?? "ws://127.0.0.1:8080";
const listeners = new Set();
const waiters = [];

let socket = null;
let holdChat = false;
let pendingChat = [];
let transcriptGen = 0;

const state = {
  link: "Disconnected",
  busy: false,
  user: null,
  rooms: [],
  currentRoom: lobbyRoom(),
  transcript: [],
};

function lobbyRoom() {
  return { id: 1, name: "Lobby", type: "LOBBY", privacy: "PUBLIC" };
}

function anonymousUser() {
  const suffix = String(Math.floor(Math.random() * 10000)).padStart(7, "0");
  const username = `anon${suffix}`;
  return { username, email: `${username}@local`, type: "GUEST" };
}

function isLoggedIn() {
  return Boolean(state.user) && state.user.type !== "GUEST";
}

function isAdmin() {
  return Boolean(state.user) && state.user.type === "ADMIN";
}

function connected() {
  return socket?.readyState === WebSocket.OPEN;
}

function snapshot() {
  const current = state.currentRoom?.name ?? "Lobby";
  const rooms = [...state.rooms].sort((a, b) => a.id - b.id);
  return {
    link: state.link,
    busy: state.busy,
    connected: state.link === "Connected",
    loggedIn: isLoggedIn(),
    admin: isAdmin(),
    username: state.user?.username ?? "Guest",
    email: state.user?.email ?? "",
    rooms: rooms.map((room) => ({
      id: room.id,
      name: room.name,
      privacy: room.privacy === "PRIVATE" ? "Private" : "Public",
      here: room.name === current,
    })),
    waiting: rooms.length === 0 && state.link === "Connected",
    currentRoom: current,
    inLobby: current === "Lobby",
    transcript: state.transcript.map((line) => ({ ...line })),
  };
}

function notify() {
  const current = snapshot();
  for (const listener of listeners) {
    listener(current);
  }
}

function roomFromName(name) {
  if (name === "Lobby") {
    return lobbyRoom();
  }
  const known = state.rooms.find((room) => room.name === name);
  if (known) {
    return { ...known };
  }
  return { id: 0, name, type: "OTHER", privacy: "PUBLIC" };
}

function applyRoomDirectory(encoded) {
  state.rooms = parseRoomList(encoded);
}

function rememberRoom(name) {
  if (!name || state.rooms.some((room) => room.name === name)) {
    return;
  }
  state.rooms.push({ id: 0, name, type: "OTHER", privacy: "PUBLIC" });
}

function applyRoomList(packet) {
  if (packet.message.startsWith("room(")) {
    applyRoomDirectory(packet.message);
  } else if (packet.room.startsWith("room(")) {
    applyRoomDirectory(packet.room);
  }

  if (packet.room && !packet.room.startsWith("room(")) {
    state.currentRoom = roomFromName(packet.room);
  }

  const userPayload = packet.receiver.startsWith("user(") ? packet.receiver : packet.sender;
  if (userPayload.startsWith("user(")) {
    const user = parseUser(userPayload);
    if (user) {
      state.user = user;
      state.transcript = [];
      pendingChat = [];
    }
  }
}

function enqueueChat(packet) {
  if (state.currentRoom && packet.room && packet.room !== state.currentRoom.name) {
    return;
  }
  const line = {
    author: packet.sender,
    text: packet.message,
    timestamp: packet.timestamp || Math.floor(Date.now() / 1000),
  };
  if (holdChat) {
    pendingChat.push(line);
    return;
  }
  state.transcript.push(line);
  notify();
}

function finishWaiter(waiter, packet) {
  const index = waiters.indexOf(waiter);
  if (index !== -1) {
    waiters.splice(index, 1);
  }
  clearTimeout(waiter.timer);
  waiter.resolve(packet);
}

function failWaiters() {
  for (const waiter of [...waiters]) {
    finishWaiter(waiter, null);
  }
}

function onFrame(event) {
  const packet = decodePacket(event.data);
  if (!packet || !connected()) {
    return;
  }

  if (packet.type === PacketType.HEARTBEAT && packet.message === "ping") {
    socket.send(encodePong());
    return;
  }

  if (packet.responseCode === 0) {
    if (packet.type === PacketType.ROOM_LIST) {
      applyRoomList(packet);
      notify();
    } else if (packet.type === PacketType.MESSAGE) {
      enqueueChat(packet);
    }
    return;
  }

  const waiter = waiters.find((item) => item.type === packet.type);
  if (waiter) {
    finishWaiter(waiter, packet);
  }
}

function openSocket() {
  if (socket && (socket.readyState === WebSocket.OPEN || socket.readyState === WebSocket.CONNECTING)) {
    return;
  }

  const current = new WebSocket(socketUrl);
  socket = current;
  current.binaryType = "arraybuffer";
  current.onopen = () => {
    if (socket !== current) {
      return;
    }
    state.link = "Connected";
    notify();
  };
  current.onmessage = onFrame;
  current.onerror = () => {
    if (socket !== current) {
      return;
    }
    state.link = "Disconnected";
    notify();
  };
  current.onclose = () => {
    if (socket !== current) {
      return;
    }
    state.link = "Disconnected";
    failWaiters();
    notify();
    if (listeners.size > 0) {
      setTimeout(openSocket, 1000);
    }
  };
}

function request(packet) {
  if (!connected()) {
    return Promise.resolve(null);
  }
  return new Promise((resolve) => {
    const waiter = { type: packet.type, resolve };
    waiter.timer = setTimeout(() => finishWaiter(waiter, null), 15000);
    waiters.push(waiter);
    socket.send(encodePacket(packet));
  });
}

function requireLink() {
  if (!connected()) {
    return "Not connected to the server.";
  }
  if (!state.user) {
    return "No user identity yet — wait for the server welcome.";
  }
  return "";
}

async function withBusy(fn) {
  if (state.busy) {
    return "busy";
  }
  state.busy = true;
  notify();
  try {
    return await fn();
  } finally {
    state.busy = false;
    notify();
  }
}

async function resetTranscript() {
  const gen = ++transcriptGen;
  holdChat = true;
  pendingChat = [];
  state.transcript = [];
  notify();

  let error = "";
  if (!connected() || !state.user || !state.currentRoom) {
    error = "No room to load history for.";
  } else {
    const response = await request({
      type: PacketType.LOAD_MESSAGE_HISTORY,
      sender: state.user.username,
      room: state.currentRoom.name,
    });
    if (gen !== transcriptGen) {
      return "";
    }
    if (!response) {
      error = "Connection lost while loading history.";
    } else if (response.responseCode !== ResponseCode.SUCCESS) {
      error = response.message || "Load history failed.";
    } else {
      state.transcript = parseHistory(response.message);
    }
  }

  if (gen !== transcriptGen) {
    return "";
  }
  if (error) {
    state.transcript = [{ author: "system", text: error, timestamp: 0 }];
  }
  state.transcript.push(...pendingChat);
  pendingChat = [];
  holdChat = false;
  notify();
  return error;
}

export function watchState(listener) {
  listeners.add(listener);
  openSocket();
  listener(snapshot());
  return () => {
    listeners.delete(listener);
    if (listeners.size === 0 && socket) {
      socket.close();
      socket = null;
    }
  };
}

export function login(username, password) {
  return withBusy(async () => {
    if (!connected()) {
      return "Not connected to the server.";
    }
    if (isLoggedIn()) {
      return "Already logged in.";
    }
    if (!username || !password) {
      return "Username and password are required";
    }

    const response = await request({
      type: PacketType.LOGIN,
      sender: username,
      message: password,
    });
    if (!response) {
      return "Connection lost while logging in.";
    }
    if (response.responseCode !== ResponseCode.SUCCESS) {
      return response.message || "Login failed.";
    }
    const user = parseUser(response.message);
    if (!user) {
      return "Login failed: invalid user payload";
    }
    state.user = user;
    state.currentRoom = lobbyRoom();
    applyRoomDirectory(response.room);
    await resetTranscript();
    return "";
  });
}

export function signUp(username, password, email) {
  return withBusy(async () => {
    if (!connected()) {
      return "Not connected to the server.";
    }
    if (isLoggedIn()) {
      return "Already logged in.";
    }
    if (!username || !password || !email) {
      return "Username, password and email are required";
    }

    const response = await request({
      type: PacketType.REGISTER,
      sender: username,
      room: email,
      message: password,
    });
    if (!response) {
      return "Connection lost while signing up.";
    }
    if (response.responseCode !== ResponseCode.SUCCESS) {
      return response.message || "Sign up failed.";
    }
    const user = parseUser(response.message);
    if (!user) {
      return "Sign up failed: invalid user payload";
    }
    state.user = user;
    state.currentRoom = lobbyRoom();
    applyRoomDirectory(response.room);
    await resetTranscript();
    return "";
  });
}

export function logout() {
  return withBusy(async () => {
    if (!connected()) {
      return "Not connected to the server.";
    }
    if (!isLoggedIn() || !state.user) {
      return "Not logged in.";
    }

    const response = await request({
      type: PacketType.LOGOUT,
      sender: state.user.username,
      message: state.user.email,
    });
    if (!response) {
      return "Connection lost while logging out.";
    }
    if (response.responseCode !== ResponseCode.SUCCESS) {
      return response.message || "Logout failed.";
    }

    const rooms = state.rooms;
    state.user = anonymousUser();
    state.rooms = rooms;
    state.currentRoom = lobbyRoom();
    await resetTranscript();
    return "";
  });
}

export function updateProfile(username, newPassword, email) {
  return withBusy(async () => {
    if (!connected()) {
      return "Not connected to the server.";
    }
    if (!isLoggedIn() || !state.user) {
      return "Log in to update your profile.";
    }
    if (!username || !email) {
      return "Username and email are required";
    }

    const response = await request({
      type: PacketType.UPDATE_USER,
      sender: username,
      room: email,
      message: newPassword,
    });
    if (!response) {
      return "Connection lost while updating profile.";
    }
    if (response.responseCode !== ResponseCode.SUCCESS) {
      return response.message || "Update profile failed.";
    }
    const user = parseUser(response.message);
    if (!user) {
      return "Update profile failed: invalid user payload";
    }
    state.user = user;
    notify();
    return "";
  });
}

export function joinRoom(roomName) {
  return withBusy(async () => {
    const blocked = requireLink();
    if (blocked) {
      return blocked;
    }
    if (!roomName) {
      return "Room name cannot be empty.";
    }

    const response = await request({
      type: PacketType.ROOM_JOIN,
      sender: state.user.username,
      room: roomName,
    });
    if (!response) {
      return "Connection lost while joining.";
    }
    if (response.responseCode !== ResponseCode.SUCCESS) {
      return response.message || "Join failed.";
    }
    state.currentRoom = roomFromName(response.room || roomName);
    await resetTranscript();
    return "";
  });
}

export function leaveRoom() {
  return withBusy(async () => {
    const blocked = requireLink();
    if (blocked) {
      return blocked;
    }
    if (!state.currentRoom) {
      return "Not in a room.";
    }
    if (state.currentRoom.name === "Lobby") {
      return "Cannot leave the Lobby.";
    }

    const response = await request({
      type: PacketType.ROOM_LEAVE,
      sender: state.user.username,
      room: state.currentRoom.name,
    });
    if (!response) {
      return "Connection lost while leaving.";
    }
    if (response.responseCode !== ResponseCode.SUCCESS) {
      return response.message || "Leave failed.";
    }
    state.currentRoom = lobbyRoom();
    await resetTranscript();
    return "";
  });
}

export function createRoom(roomName, privacy) {
  return withBusy(async () => {
    if (!connected()) {
      return "Not connected to the server.";
    }
    if (!isLoggedIn() || !state.user) {
      return "Log in to create a room.";
    }
    if (!roomName) {
      return "Username and room are required";
    }

    const response = await request({
      type: PacketType.ROOM_CREATE,
      sender: state.user.username,
      room: roomName,
      message: privacy,
    });
    if (!response) {
      return "Connection lost while creating room.";
    }
    if (response.responseCode !== ResponseCode.SUCCESS) {
      return response.message || "Create room failed.";
    }
    rememberRoom(response.room);
    notify();
    return "";
  });
}

export function inviteToRoom(inviteeUsername) {
  return withBusy(async () => {
    if (!connected()) {
      return "Not connected to the server.";
    }
    if (!isLoggedIn() || !state.user) {
      return "Log in to invite users.";
    }
    if (!state.currentRoom || state.currentRoom.name === "Lobby") {
      return "Join a room before inviting.";
    }
    if (!inviteeUsername) {
      return "Username, room and invitee are required";
    }

    const response = await request({
      type: PacketType.ROOM_INVITE,
      sender: state.user.username,
      room: state.currentRoom.name,
      message: inviteeUsername,
    });
    if (!response) {
      return "Connection lost while inviting.";
    }
    if (response.responseCode !== ResponseCode.SUCCESS) {
      return response.message || "Invite failed.";
    }
    return "";
  });
}

export function kickFromRoom(targetUsername) {
  return withBusy(async () => {
    if (!connected()) {
      return "Not connected to the server.";
    }
    if (!isLoggedIn() || !state.user) {
      return "Log in to kick users.";
    }
    if (!state.currentRoom || state.currentRoom.name === "Lobby") {
      return "Join a room before kicking.";
    }
    if (!targetUsername) {
      return "Username, room and target are required";
    }

    const response = await request({
      type: PacketType.ROOM_KICK,
      sender: state.user.username,
      room: state.currentRoom.name,
      message: targetUsername,
    });
    if (!response) {
      return "Connection lost while kicking.";
    }
    if (response.responseCode !== ResponseCode.SUCCESS) {
      return response.message || "Kick failed.";
    }
    return "";
  });
}

export function deleteRoom(roomName) {
  return withBusy(async () => {
    if (!connected()) {
      return "Not connected to the server.";
    }
    if (!isAdmin() || !state.user) {
      return "Admin required to delete a room.";
    }
    if (!roomName) {
      return "Username and room are required";
    }

    const response = await request({
      type: PacketType.ROOM_DELETE,
      sender: state.user.username,
      room: roomName,
    });
    if (!response) {
      return "Connection lost while deleting room.";
    }
    if (response.responseCode !== ResponseCode.SUCCESS) {
      return response.message || "Delete room failed.";
    }
    if (state.currentRoom && state.currentRoom.name === roomName) {
      state.currentRoom = lobbyRoom();
    }
    await resetTranscript();
    return "";
  });
}

export function sendMessage(text) {
  return withBusy(async () => {
    const blocked = requireLink();
    if (blocked) {
      return blocked;
    }
    if (!text) {
      return "Message cannot be empty.";
    }

    const response = await request({
      type: PacketType.MESSAGE,
      sender: state.user.username,
      message: text,
    });
    if (!response) {
      return "Connection lost while sending.";
    }
    if (response.responseCode !== ResponseCode.SUCCESS) {
      return response.message || "Send failed.";
    }
    return "";
  });
}
