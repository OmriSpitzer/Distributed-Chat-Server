/**
 * User service
 * @brief Sign-in, profile, and checks that block a user action.
 * @date 2026-09-29
 */

import { parseUser, PacketType } from "./packet.js";
import {
  anonymousUser, applyRoomDirectory, connected, lobbyRoom, notify, refused, request,
  resetTranscript, session, withBusy,
} from "./chatService.js";

export function isLoggedIn() {
  return Boolean(session.user) && session.user.type !== "GUEST";
}

// "" when this browser may sign in. Otherwise why it is blocked.
export function blockedSignIn() {
  if (!connected()) {
    return "Not connected to the server.";
  }
  if (isLoggedIn()) {
    return "Already logged in.";
  }
  return "";
}

// "" when a signed-in account may run the action.
export function blockedUser(message) {
  if (!connected()) {
    return "Not connected to the server.";
  }
  if (!isLoggedIn() || !session.user) {
    return message;
  }
  return "";
}

// "" when the welcome user exists, including a guest.
export function blockedIdentity() {
  if (!connected()) {
    return "Not connected to the server.";
  }
  if (!session.user) {
    return "No user identity yet — wait for the server welcome.";
  }
  return "";
}

export function login(username, password) {
  return withBusy(async () => {
    const blocked = blockedSignIn();
    if (blocked) {
      return blocked;
    }
    if (!username || !password) {
      return "Username and password are required";
    }

    const response = await request({
      type: PacketType.LOGIN,
      sender: username,
      message: password,
    });
    const error = refused(response, "Connection lost while logging in.", "Login failed.");
    if (error) {
      return error;
    }

    const user = parseUser(response.message);
    if (!user) {
      return "Login failed: invalid user payload";
    }
    session.user = user;
    session.currentRoom = lobbyRoom();
    applyRoomDirectory(response.room);
    await resetTranscript();
    return "";
  });
}

export function signUp(username, password, email) {
  return withBusy(async () => {
    const blocked = blockedSignIn();
    if (blocked) {
      return blocked;
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
    const error = refused(response, "Connection lost while signing up.", "Sign up failed.");
    if (error) {
      return error;
    }

    const user = parseUser(response.message);
    if (!user) {
      return "Sign up failed: invalid user payload";
    }
    session.user = user;
    session.currentRoom = lobbyRoom();
    applyRoomDirectory(response.room);
    await resetTranscript();
    return "";
  });
}

export function logout() {
  return withBusy(async () => {
    const blocked = blockedUser("Not logged in.");
    if (blocked) {
      return blocked;
    }

    const response = await request({
      type: PacketType.LOGOUT,
      sender: session.user.username,
      message: session.user.email,
    });
    const error = refused(response, "Connection lost while logging out.", "Logout failed.");
    if (error) {
      return error;
    }

    const rooms = session.rooms;
    session.user = anonymousUser();
    session.rooms = rooms;
    session.currentRoom = lobbyRoom();
    await resetTranscript();
    return "";
  });
}

export function updateProfile(username, newPassword, email) {
  return withBusy(async () => {
    const blocked = blockedUser("Log in to update your profile.");
    if (blocked) {
      return blocked;
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
    const error = refused(response, "Connection lost while updating profile.", "Update profile failed.");
    if (error) {
      return error;
    }

    const user = parseUser(response.message);
    if (!user) {
      return "Update profile failed: invalid user payload";
    }
    session.user = user;
    notify();
    return "";
  });
}
