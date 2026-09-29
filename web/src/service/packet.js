// Values match Packet::PacketType in include/utils/models/packet.h
export const PacketType = {
  LOGIN: 0,
  LOGOUT: 1,
  MESSAGE: 2,
  ROOM_JOIN: 3,
  ROOM_LEAVE: 4,
  DEFAULT: 5,
  HEARTBEAT: 6,
  REGISTER: 7,
  GOSSIP_HELLO: 8,
  GOSSIP_EVENT: 9,
  GOSSIP_DIGEST: 10,
  GOSSIP_PULL: 11,
  UPDATE_USER: 12,
  ROOM_CREATE: 13,
  ROOM_LIST: 14,
  LOAD_MESSAGE_HISTORY: 15,
  ROOM_INVITE: 16,
  ROOM_DELETE: 17,
  ROOM_KICK: 18,
  SERVER_DIRECTORY: 19,
};

function appendU32(bytes, value) {
  bytes.push((value >>> 24) & 0xff, (value >>> 16) & 0xff, (value >>> 8) & 0xff, value & 0xff);
}

function appendU64(bytes, value) {
  const high = Math.floor(value / 2 ** 32);
  const low = value >>> 0;
  appendU32(bytes, high);
  appendU32(bytes, low);
}

function appendString(bytes, text) {
  const encoded = new TextEncoder().encode(text);
  appendU32(bytes, encoded.length);
  for (const byte of encoded) {
    bytes.push(byte);
  }
}

function readU32(bytes, offset) {
  if (offset + 4 > bytes.length) {
    return null;
  }
  const value =
    ((bytes[offset] << 24) | (bytes[offset + 1] << 16) | (bytes[offset + 2] << 8) | bytes[offset + 3]) >>>
    0;
  return { value, offset: offset + 4 };
}

function readU64(bytes, offset) {
  const high = readU32(bytes, offset);
  if (!high) {
    return null;
  }
  const low = readU32(bytes, high.offset);
  if (!low) {
    return null;
  }
  return { value: high.value * 2 ** 32 + low.value, offset: low.offset };
}

function readString(bytes, offset) {
  const length = readU32(bytes, offset);
  if (!length || length.offset + length.value > bytes.length) {
    return null;
  }
  const text = new TextDecoder().decode(bytes.subarray(length.offset, length.offset + length.value));
  return { value: text, offset: length.offset + length.value };
}

// Framed packet: [u32 BE size][u8 type][u64 BE timestamp][u32 BE responseCode][len-prefixed fields]
export function encodePacket({ type, sender, receiver = "", room = "", message = "", responseCode = 0 }) {
  const payload = [];
  payload.push(type & 0xff);
  appendU64(payload, Math.floor(Date.now() / 1000));
  appendU32(payload, responseCode >>> 0);
  appendString(payload, sender);
  appendString(payload, receiver);
  appendString(payload, room);
  appendString(payload, message);

  const framed = [];
  appendU32(framed, payload.length);
  framed.push(...payload);
  return new Uint8Array(framed);
}

export function encodePong() {
  return encodePacket({
    type: PacketType.HEARTBEAT,
    sender: "client",
    receiver: "server",
    message: "pong",
  });
}

export function decodePacket(buffer) {
  const bytes = buffer instanceof Uint8Array ? buffer : new Uint8Array(buffer);
  const size = readU32(bytes, 0);
  if (!size || bytes.length !== 4 + size.value) {
    return null;
  }

  let offset = 4;
  if (offset >= bytes.length) {
    return null;
  }
  const type = bytes[offset];
  offset += 1;

  const timestamp = readU64(bytes, offset);
  if (!timestamp) {
    return null;
  }
  const responseCode = readU32(bytes, timestamp.offset);
  if (!responseCode) {
    return null;
  }
  const sender = readString(bytes, responseCode.offset);
  const receiver = sender && readString(bytes, sender.offset);
  const room = receiver && readString(bytes, receiver.offset);
  const message = room && readString(bytes, room.offset);
  if (!message || message.offset !== bytes.length) {
    return null;
  }

  return {
    type,
    timestamp: timestamp.value,
    responseCode: responseCode.value,
    sender: sender.value,
    receiver: receiver.value,
    room: room.value,
    message: message.value,
  };
}

// user(username|email|user_type)
export function parseUser(serialized) {
  if (!serialized.startsWith("user(") || !serialized.endsWith(")")) {
    return null;
  }
  const body = serialized.slice(5, -1);
  const first = body.indexOf("|");
  const second = first === -1 ? -1 : body.indexOf("|", first + 1);
  if (first === -1 || second === -1) {
    return null;
  }
  return {
    username: body.slice(0, first),
    email: body.slice(first + 1, second),
    type: body.slice(second + 1),
  };
}

// room(id|name|type|privacy)
export function parseRoom(serialized) {
  if (!serialized.startsWith("room(") || !serialized.endsWith(")")) {
    return null;
  }
  const body = serialized.slice(5, -1);
  const first = body.indexOf("|");
  const second = first === -1 ? -1 : body.indexOf("|", first + 1);
  const third = second === -1 ? -1 : body.indexOf("|", second + 1);
  if (first === -1 || second === -1 || third === -1) {
    return null;
  }
  const id = Number(body.slice(0, first));
  if (!Number.isInteger(id)) {
    return null;
  }
  return {
    id,
    name: body.slice(first + 1, second),
    type: body.slice(second + 1, third),
    privacy: body.slice(third + 1),
  };
}

// room(...);room(...)
export function parseRoomList(serialized) {
  if (!serialized) {
    return [];
  }
  const rooms = [];
  for (const piece of serialized.split(";")) {
    if (!piece) {
      continue;
    }
    const room = parseRoom(piece);
    if (room) {
      rooms.push(room);
    }
  }
  return rooms;
}

// "[user][unix] text" lines, plus the legacy "[user] text" form
export function parseHistory(payload) {
  const lines = [];
  for (const line of payload.split("\n")) {
    if (!line) {
      continue;
    }
    if (line.startsWith("[")) {
      const closeUser = line.indexOf("]");
      if (closeUser !== -1) {
        const author = line.slice(1, closeUser);
        let rest = line.slice(closeUser + 1);
        let timestamp = 0;
        if (rest.startsWith("[")) {
          const closeTs = rest.indexOf("]");
          if (closeTs !== -1) {
            const parsed = Number(rest.slice(1, closeTs));
            timestamp = Number.isFinite(parsed) ? parsed : 0;
            rest = rest.slice(closeTs + 1);
          }
        }
        if (rest.startsWith(" ")) {
          rest = rest.slice(1);
        }
        lines.push({ author, text: rest, timestamp });
        continue;
      }
    }
    lines.push({ author: "system", text: line, timestamp: 0 });
  }
  return lines;
}
