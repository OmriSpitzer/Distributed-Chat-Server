/**
 * Room service
 * @brief Join, leave, create, invite, and send.
 * @date 2026-09-29
 */

import { PacketType } from "./packet.js";
import {
  lobbyRoom, notify, refused, rememberRoom, request, resetTranscript, roomFromName, session, withBusy,
} from "./chatService.js";
import { blockedIdentity, blockedUser } from "./userService.js";

export function joinRoom(roomName) {
  return withBusy(async () => {
    const blocked = blockedIdentity();
    if (blocked) {
      return blocked;
    }
    if (!roomName) {
      return "Room name cannot be empty.";
    }

    const response = await request({
      type: PacketType.ROOM_JOIN,
      sender: session.user.username,
      room: roomName,
    });
    const error = refused(response, "Connection lost while joining.", "Join failed.");
    if (error) {
      return error;
    }
    session.currentRoom = roomFromName(response.room || roomName);
    await resetTranscript();
    return "";
  });
}

export function leaveRoom() {
  return withBusy(async () => {
    const blocked = blockedIdentity();
    if (blocked) {
      return blocked;
    }
    if (!session.currentRoom) {
      return "Not in a room.";
    }
    if (session.currentRoom.name === "Lobby") {
      return "Cannot leave the Lobby.";
    }

    const response = await request({
      type: PacketType.ROOM_LEAVE,
      sender: session.user.username,
      room: session.currentRoom.name,
    });
    const error = refused(response, "Connection lost while leaving.", "Leave failed.");
    if (error) {
      return error;
    }
    session.currentRoom = lobbyRoom();
    await resetTranscript();
    return "";
  });
}

export function createRoom(roomName, privacy) {
  return withBusy(async () => {
    const blocked = blockedUser("Log in to create a room.");
    if (blocked) {
      return blocked;
    }
    if (!roomName) {
      return "Username and room are required";
    }

    const response = await request({
      type: PacketType.ROOM_CREATE,
      sender: session.user.username,
      room: roomName,
      message: privacy,
    });
    const error = refused(response, "Connection lost while creating room.", "Create room failed.");
    if (error) {
      return error;
    }
    rememberRoom(response.room);
    notify();
    return "";
  });
}

export function inviteToRoom(inviteeUsername) {
  return withBusy(async () => {
    const blocked = blockedUser("Log in to invite users.");
    if (blocked) {
      return blocked;
    }
    if (!session.currentRoom || session.currentRoom.name === "Lobby") {
      return "Join a room before inviting.";
    }
    if (!inviteeUsername) {
      return "Username, room and invitee are required";
    }

    const response = await request({
      type: PacketType.ROOM_INVITE,
      sender: session.user.username,
      room: session.currentRoom.name,
      message: inviteeUsername,
    });
    return refused(response, "Connection lost while inviting.", "Invite failed.");
  });
}

export function sendMessage(text) {
  return withBusy(async () => {
    const blocked = blockedIdentity();
    if (blocked) {
      return blocked;
    }
    if (!text) {
      return "Message cannot be empty.";
    }

    const response = await request({
      type: PacketType.MESSAGE,
      sender: session.user.username,
      message: text,
    });
    return refused(response, "Connection lost while sending.", "Send failed.");
  });
}
