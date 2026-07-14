/**
 * RoomManager class
 *
 * @brief Basic room lifecycle operations backed by the database.
 * @date 14-07-2026
 */

#include "server/room_manager.h"
#include "server/database_manager.h"
#include <iostream>

bool RoomManager::createRoom(const Room &room) {
  return DatabaseManager::getInstance().createRoom(room);
}

bool RoomManager::deleteRoom(const Room &room) {
  std::cout << "Deleting room: " << room.getName() << '\n';
  return true;
}

bool RoomManager::joinRoom(const Room &room, const User &user) {
  std::cout << user.getUsername() << " joined room " << room.getName() << '\n';
  return true;
}

bool RoomManager::leaveRoom(const Room &room, const User &user) {
  std::cout << user.getUsername() << " left room " << room.getName() << '\n';
  return true;
}

bool RoomManager::broadcast(const Room &room, const std::string &message) {
  std::cout << "[" << room.getName() << "] " << message << '\n';
  return true;
}
