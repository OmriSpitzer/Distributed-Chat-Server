/**
 * ClientState header file class
 *
 * @date 13-09-2026
 */

#pragma once
#include "utils/models/room.h"
#include "utils/models/user.h"
#include <mutex>
#include <optional>
#include <vector>

class ClientState {
public:
  // non-copyable / non-movable (mutex)
  ClientState() = default;
  ClientState(const ClientState &) = delete;
  ClientState &operator=(const ClientState &) = delete;

  // authenticated (non-guest) user present
  bool isLoggedIn() const {
    return user.has_value() && user->getUserType() != User::UserType::GUEST;
  }

  // replace the cached room directory (thread-safe)
  void setRooms(std::vector<Room> next) {
    std::lock_guard<std::mutex> lock(roomsMutex);
    rooms = std::move(next);
  }

  // snapshot of the cached room directory (thread-safe)
  std::vector<Room> getRooms() const {
    std::lock_guard<std::mutex> lock(roomsMutex);
    return rooms;
  }

  // clear the data
  void clear() {
    user.reset();
    currentRoom.reset();
    std::lock_guard<std::mutex> lock(roomsMutex);
    rooms.clear();
  }

  std::optional<User> user;        // user data
  std::optional<Room> currentRoom; // current room data
private:
  std::vector<Room> rooms;       // cached room directory
  mutable std::mutex roomsMutex; // guards rooms
};
