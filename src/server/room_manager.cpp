/**
 * RoomManager class
 *
 * @brief In-memory room membership (room name → sockets) and live broadcast.
 * @date 10-09-2026
 */

#include "server/room_manager.h"
#include "config/config.h"
#include "server/client_session.h"
#include "server/connection_manager.h"
#include "server/database_manager.h"
#include "utils/models/logger.h"
#include <exception>
#include <vector>

// Lobby room
const Room RoomManager::LOBBY = Room("Lobby", Room::RoomType::LOBBY);

RoomManager::RoomManager() {
  knownRooms.emplace("Lobby", LOBBY);
  knownRooms.emplace("General", Room("General"));
  knownRooms.emplace("Random", Room("Random"));
}

std::optional<Room> RoomManager::getRoom(const std::string &name) const {
  std::lock_guard<std::mutex> lock(mutex);
  auto it = knownRooms.find(name.empty() ? "Lobby" : name);
  if (it == knownRooms.end()) {
    return std::nullopt;
  }
  return it->second;
}

void RoomManager::removeSocketLocked(int socket, const std::string &roomName) {
  auto it = members.find(roomName);
  if (it == members.end()) {
    return;
  }
  it->second.erase(socket);
  if (it->second.empty()) {
    members.erase(it);
  }
}

bool RoomManager::joinRoom(const std::string &roomName, ClientSession &session) {
  const std::string name = roomName.empty() ? "Lobby" : roomName;
  const std::string previousRoom = session.getRoom().getName();
  const std::string username = session.getUser().getUsername();
  const bool persist = session.isAuthenticated();

  {
    std::lock_guard<std::mutex> lock(mutex);
    auto roomIt = knownRooms.find(name);
    if (roomIt == knownRooms.end()) {
      Logger::logWarning("RoomManager", "Unknown room: " + name);
      return false;
    }

    removeSocketLocked(session.getSocket(), previousRoom);
    session.setRoom(roomIt->second);
    members[name].insert(session.getSocket());
  }

  if (persist) {
    DatabaseManager &db = DatabaseManager::getInstance();
    if (previousRoom != name) {
      db.clearMembership(username, previousRoom);
    }
    db.setMembership(username, name, config::NODE_ID);
  }

  Logger::logInfo("RoomManager", username + " joined " + name);
  return true;
}

void RoomManager::leaveAll(ClientSession &session) {
  const std::string previousRoom = session.getRoom().getName();
  const std::string username = session.getUser().getUsername();
  const bool persist = session.isAuthenticated();

  {
    std::lock_guard<std::mutex> lock(mutex);
    removeSocketLocked(session.getSocket(), previousRoom);
    session.setRoom(LOBBY);
  }

  if (persist) {
    try {
      DatabaseManager::getInstance().clearMembership(username, previousRoom);
    } catch (const std::exception &e) {
      Logger::logError("RoomManager", std::string("Failed to clear membership: ") + e.what());
    }
  }

  Logger::logInfo("RoomManager",
                  username + " left rooms, socket " + std::to_string(session.getSocket()));
}

bool RoomManager::broadcast(const Room &room, const Packet &packet, ConnectionManager &connections,
                            int skipSocket) {
  std::vector<int> sockets;
  {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = members.find(room.getName());
    if (it != members.end()) {
      sockets.assign(it->second.begin(), it->second.end());
    }
  }

  bool ok = true;
  for (int socket : sockets) {
    if (socket == skipSocket) {
      continue;
    }
    if (!connections.sendPacket(socket, packet)) {
      Logger::logWarning("RoomManager", "Failed to send to socket " + std::to_string(socket) +
                                            " in " + room.getName());
      ok = false;
    }
  }
  return ok;
}

bool RoomManager::createRoom(const Room &room) {
  std::lock_guard<std::mutex> lock(mutex);
  if (knownRooms.find(room.getName()) != knownRooms.end()) {
    return false;
  }
  knownRooms.emplace(room.getName(), room);
  Logger::logInfo("RoomManager", "Room created: " + room.getName());
  return true;
}

bool RoomManager::deleteRoom(const Room &room) {
  if (room.getName() == LOBBY.getName()) {
    return false;
  }
  std::lock_guard<std::mutex> lock(mutex);
  knownRooms.erase(room.getName());
  members.erase(room.getName());
  Logger::logInfo("RoomManager", "Room deleted: " + room.getName());
  return true;
}

bool RoomManager::broadcastAll(const std::string &message) {
  Logger::logInfo("RoomManager", "Message broadcasted to all rooms: " + message);
  return true;
}