/**
 * ClientSession header file class
 *
 * @date 07-09-2026
 */
#pragma once
#include "utils/models/room.h"
#include "utils/models/user.h"
#include <chrono>

class ClientSession {
public:
  // constructor
  ClientSession(int socket, const User &user, const Room &room);

  // getters
  int getSocket() const;
  const User &getUser() const;
  const Room &getRoom() const;
  bool isAuthenticated() const;

  // setters
  void setUser(const User &user);
  void setRoom(const Room &room);
  void setAuthenticated(bool value);

  // last heartbeat timestamp
  std::chrono::steady_clock::time_point lastHeartbeatTime;

  // touch the last heartbeat time
  void touch();

  // is the client alive
  bool isAlive() const;

private:
  // client socket
  int clientSocket;

  // user
  User user;

  // is the client authenticated
  bool authenticated;

  // current room
  Room currentRoom;
};