/**
 * Admin service
 * @brief Kick, delete, and the check that blocks a non-admin.
 * @date 2026-09-29
 */

import { PacketType } from "./packet.js";
import {
  lobbyRoom, refused, request, resetTranscript, session, withBusy,
} from "./chatService.js";
import { blockedUser } from "./userService.js";

export function isAdmin() {
  return Boolean(session.user) && session.user.type === "ADMIN";
}

// "" when the signed-in user is an admin.
export function blockedAdmin(message) {
  const blocked = blockedUser(message);
  if (blocked) {
    return blocked;
  }
  if (!isAdmin()) {
    return message;
  }
  return "";
}

export function kickFromRoom(targetUsername) {
  return withBusy(async () => {
    const signedOut = blockedUser("Log in to kick users.");
    if (signedOut) {
      return signedOut;
    }
    const blocked = blockedAdmin("Admin required to kick a user.");
    if (blocked) {
      return blocked;
    }
    if (!session.currentRoom || session.currentRoom.name === "Lobby") {
      return "Join a room before kicking.";
    }
    if (!targetUsername) {
      return "Username, room and target are required";
    }

    const response = await request({
      type: PacketType.ROOM_KICK,
      sender: session.user.username,
      room: session.currentRoom.name,
      message: targetUsername,
    });
    return refused(response, "Connection lost while kicking.", "Kick failed.");
  });
}

export function deleteRoom(roomName) {
  return withBusy(async () => {
    const blocked = blockedAdmin("Admin required to delete a room.");
    if (blocked) {
      return blocked;
    }
    if (!roomName) {
      return "Username and room are required";
    }

    const response = await request({
      type: PacketType.ROOM_DELETE,
      sender: session.user.username,
      room: roomName,
    });
    const error = refused(response, "Connection lost while deleting room.", "Delete room failed.");
    if (error) {
      return error;
    }
    if (session.currentRoom && session.currentRoom.name === roomName) {
      session.currentRoom = lobbyRoom();
    }
    await resetTranscript();
    return "";
  });
}
