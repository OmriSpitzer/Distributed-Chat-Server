/**
 * ClientSession header file class
 *
 * @date 11-09-2026
 */
#pragma once
#include "utils/models/room.h"
#include "utils/models/user.h"
#include <atomic>
#include <chrono>
#include <mutex>


class ClientSession {
public:
  // constructor
  ClientSession(int socket, const User &user, const Room &room);

  // delete copy constructor and assignment operator
  ClientSession(const ClientSession &) = delete;
  ClientSession &operator=(const ClientSession &) = delete;

  // getters
  int getSocket() const;
  User getUser() const;
  Room getRoom() const;
  bool isAuthenticated() const;
  bool isClosed() const;
  std::mutex &sendMutex();

  // setters
  void setUser(const User &user);
  void setRoom(const Room &room);
  void setAuthenticated(bool value);

  // mark the client session as closed
  bool markClosed();

  // touch the last heartbeat time
  void touch();

  // is the client alive
  bool isAlive() const;

private:
  User user;        // user
  Room room;        // room
  int clientSocket; // client socket

  std::mutex sendMutex_;            // send mutex
  mutable std::mutex stateMutex_;   // state mutex
  std::atomic<bool> closed_{false}; // is the client session closed
  bool authenticated;               // is the client session authenticated

  std::chrono::steady_clock::time_point lastHeartbeatTime; // last heartbeat timestamp
};