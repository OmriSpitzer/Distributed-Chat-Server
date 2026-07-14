/**
 * ClientState header file class
 *
 * @date 14-07-2026
 */
#pragma once
#include "utils/models/room.h"
#include "utils/models/user.h"
#include <string>

class ClientState {
public:
  // user
  User user;

  // current room
  Room currentRoom;

  // connected
  bool connected;

  // logged in
  bool loggedIn;
};