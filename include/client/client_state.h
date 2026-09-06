/**
 * ClientState header file class
 *
 * @date 06-09-2026
 */
#pragma once
#include "utils/models/room.h"
#include "utils/models/user.h"
#include <optional>

class ClientState {
public:
  // user
  std::optional<User> user;

  // current room
  std::optional<Room> currentRoom;

  // connected
  bool connected = false;

  // logged in
  bool loggedIn = false;
};