/**
 * RoomManager header file class
 *
 * @date 06-09-2026
 */
#pragma once
#include "utils/models/room.h"
#include "utils/models/user.h"
#include <string>

class RoomManager {
public:
  // create a room
  bool createRoom(const Room &room);

  // delete a room
  bool deleteRoom(const Room &room);

  // join a room
  bool joinRoom(const Room &room, const User &user);

  // leave a room
  bool leaveRoom(const Room &room, const User &user);

  // broadcast a message to a room
  bool broadcast(const Room &room, const std::string &message);

  // broadcast a message to all rooms
  bool broadcastAll(const std::string &message);

  // Lobby room
  static const Room LOBBY;
};
