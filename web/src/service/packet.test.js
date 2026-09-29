import assert from "node:assert/strict";
import test from "node:test";
import {
  decodePacket,
  encodePacket,
  encodePong,
  parseHistory,
  parseRoomList,
  parseUser,
  PacketType,
} from "./packet.js";

test("framed packet round-trips login fields", () => {
  const bytes = encodePacket({
    type: PacketType.LOGIN,
    sender: "ada",
    receiver: "",
    room: "",
    message: "secret",
  });
  const packet = decodePacket(bytes);
  assert.equal(packet.type, PacketType.LOGIN);
  assert.equal(packet.sender, "ada");
  assert.equal(packet.receiver, "");
  assert.equal(packet.room, "");
  assert.equal(packet.message, "secret");
  assert.equal(packet.responseCode, 0);
});

test("heartbeat pong matches the desktop client", () => {
  const packet = decodePacket(encodePong());
  assert.equal(packet.type, PacketType.HEARTBEAT);
  assert.equal(packet.sender, "client");
  assert.equal(packet.receiver, "server");
  assert.equal(packet.message, "pong");
});

test("user and room directory payloads match the C++ serializers", () => {
  assert.deepEqual(parseUser("user(ada|ada@x.com|ADMIN)"), {
    username: "ada",
    email: "ada@x.com",
    type: "ADMIN",
  });
  assert.deepEqual(parseRoomList("room(1|Lobby|LOBBY|PUBLIC);room(2|General|OTHER|PRIVATE)"), [
    { id: 1, name: "Lobby", type: "LOBBY", privacy: "PUBLIC" },
    { id: 2, name: "General", type: "OTHER", privacy: "PRIVATE" },
  ]);
});

test("history lines keep author, unix time, and text", () => {
  assert.deepEqual(parseHistory("[ada][1700000000] hi\n[bob] yo\n"), [
    { author: "ada", text: "hi", timestamp: 1700000000 },
    { author: "bob", text: "yo", timestamp: 0 },
  ]);
});
