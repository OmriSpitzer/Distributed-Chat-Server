/**
 * ClientState header file class
 *
 * @date 13-09-2026
 */

#pragma once
#include "utils/models/room.h"
#include "utils/models/user.h"
#include <optional>

class ClientState {
public:
  std::optional<User> user;        // user data
  std::optional<Room> currentRoom; // current room data

  // check if the user is logged in
  bool isLoggedIn() const { return user.has_value(); }

  // clear the data
  void clear() {
    user.reset();
    currentRoom.reset();
  }
};