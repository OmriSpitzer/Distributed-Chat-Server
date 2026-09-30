/**
 * Room Manager class implementation file (Singleton)
 *
 * @brief In-memory room membership and live broadcast
 *
 * Desgin pattern: Singleton
 * RoomManager class with fields: mutex, members, knownRooms, LOBBY, GENERAL
 * Used for managing room membership and live broadcast
 *
 * @date 13-09-2026
 */

#include "server/room_manager.h"
#include "config/config.h"
#include "server/client_session.h"
#include "server/connection_manager.h"
#include "server/database_manager.h"
#include "utils/logger/logger.h"
#include "utils/models/packet.h"
#include "utils/models/room.h"
#include <exception>
#include <vector>

// default rooms
const Room RoomManager::LOBBY(1, "Lobby", Room::RoomType::LOBBY);
const Room RoomManager::GENERAL(2, "General", Room::RoomType::OTHER);

// constructor
RoomManager::RoomManager() {
  DatabaseManager &db = DatabaseManager::getInstance();
  const std::vector<Room> defaultRooms = db.listRooms();
  for (const Room &room : defaultRooms) {
    knownRooms.emplace(room.getName(), room);
  }
}

// room getter
std::optional<Room> RoomManager::getRoom(const std::string &name) const {
  std::lock_guard<std::mutex> lock(mutex);

  // find the room
  auto it = knownRooms.find(name.empty() ? LOBBY.getName() : name);
  if (it == knownRooms.end()) {
    return std::nullopt;
  }
  return it->second;
}

// remove a socket from a room
void RoomManager::removeSocketLocked(SOCKET socket, const std::string &roomName) {
  // find the room
  auto it = members.find(roomName);
  if (it == members.end()) {
    return;
  }

  // remove the socket from the room
  it->second.erase(socket);
  if (it->second.empty()) {
    members.erase(it);
  }
}

// join a room
bool RoomManager::joinRoom(const std::string &roomName, ClientSession &session) {
  const std::string name = roomName.empty() ? LOBBY.getName() : roomName; // get the room name
  const int previousRoomId = session.getRoom().getId();         // get the previous room id
  const std::string previousRoom = session.getRoom().getName(); // get the previous room name
  const std::string username = session.getUser().getUsername(); // get the username
  const bool persist = session.isAuthenticated(); // check if the user is authenticated
  const SOCKET socket = session.getSocket();      // get the socket
  int joinedRoomId = LOBBY.getId();               // get the joined room id

  // join the room
  {
    std::lock_guard<std::mutex> lock(mutex);

    // check if the room exists
    auto roomIt = knownRooms.find(name);
    if (roomIt == knownRooms.end()) {
      Logger::logWarning("RoomManager", "Unknown room: " + name);
      return false;
    }

    // remove the socket from the previous room
    removeSocketLocked(socket, previousRoom);

    // set the room id and insert the socket into the room
    session.setRoom(roomIt->second);
    joinedRoomId = roomIt->second.getId();
    members[name].insert(socket);
  }

  // persist the membership
  if (persist) {
    try {
      DatabaseManager &db = DatabaseManager::getInstance();

      // clear the membership from the previous room
      if (previousRoomId != joinedRoomId) {
        db.clearMembership(username, previousRoomId);
      }

      // set the membership in the database
      db.setMembership(username, joinedRoomId, config::NODE_ID);
    } catch (const std::exception &e) {
      Logger::logError("RoomManager", "Failed to persist membership: " + std::string(e.what()));

      // roll back in-memory join so session/DB stay aligned
      {
        std::lock_guard<std::mutex> lock(mutex);
        removeSocketLocked(socket, name);
        auto prevIt = knownRooms.find(previousRoom);
        if (prevIt != knownRooms.end()) {
          session.setRoom(prevIt->second);
          members[previousRoom].insert(socket);
        } else {
          session.setRoom(LOBBY);
          members[LOBBY.getName()].insert(socket);
        }
      }
      return false;
    }
  }

  Logger::logInfo("RoomManager", username + " joined " + name);
  return true;
}

// leave all rooms
void RoomManager::leaveAll(ClientSession &session) {
  const std::string previousRoom = session.getRoom().getName(); // get the previous room name
  const std::string username = session.getUser().getUsername(); // get the username
  const bool persist = session.isAuthenticated(); // check if the user is authenticated
  const SOCKET socket = session.getSocket();      // get the socket

  // leave the room
  {
    std::lock_guard<std::mutex> lock(mutex);
    removeSocketLocked(socket, previousRoom);
    session.setRoom(LOBBY);
  }

  // clear all persisted memberships for this user
  if (persist && !username.empty()) {
    try {
      DatabaseManager::getInstance().clearAllMembership(username);
    } catch (const std::exception &e) {
      Logger::logError("RoomManager", std::string("Failed to clear membership: ") + e.what());
    }
  }

  Logger::logInfo("RoomManager", username + " left rooms, socket " + std::to_string(socket));
}

// broadcast a packet to all members of the room
bool RoomManager::broadcast(const Room &room, const Packet &packet, ConnectionManager &connections,
                            SOCKET skipSocket) {
  std::vector<SOCKET> sockets;

  // get the sockets
  {
    std::lock_guard<std::mutex> lock(mutex);

    // find the sockets in the room
    auto it = members.find(room.getName());
    if (it != members.end()) {
      sockets.assign(it->second.begin(), it->second.end());
    }
  }

  // broadcast the packet to the sockets
  bool ok = true;
  for (SOCKET socket : sockets) {
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

// create a new room
bool RoomManager::createRoom(const Room &room, std::string_view creatorEmail) {
  std::lock_guard<std::mutex> lock(mutex);

  // check if the room already exists
  if (knownRooms.find(room.getName()) != knownRooms.end()) {
    return false;
  }

  try {
    // create the room in the database
    Room created = DatabaseManager::getInstance().createRoom(room.getName(), room.getType(),
                                                             room.getPrivacy(), creatorEmail);
    // insert the room into the known rooms
    knownRooms.emplace(created.getName(), created);

    // log the room creation
    Logger::logInfo("RoomManager", "Room created: " + created.getName() +
                                       " id=" + std::to_string(created.getId()));
    return true;
  } catch (const std::exception &e) {
    Logger::logError("RoomManager", "Failed to create room: " + std::string(e.what()));
    return false;
  }
}

// delete an existing room
bool RoomManager::deleteRoom(const std::string &roomName, ConnectionManager &connections) {
  // check if the room is a default room
  if (roomName == LOBBY.getName() || roomName == GENERAL.getName()) {
    return false;
  }

  // delete the room
  int deletedRoomId = 0;
  std::vector<SOCKET> sockets;
  {
    std::lock_guard<std::mutex> lock(mutex);
    auto roomIt = knownRooms.find(roomName);
    if (roomIt == knownRooms.end()) {
      return false;
    }
    deletedRoomId = roomIt->second.getId();

    auto memIt = members.find(roomName);

    // get the sockets
    if (memIt != members.end()) {
      sockets.assign(memIt->second.begin(), memIt->second.end());
      members.erase(memIt);
    }

    // remove the room from memory
    knownRooms.erase(roomIt);

    // move the sockets to the lobby
    for (SOCKET socket : sockets) {
      members[LOBBY.getName()].insert(socket);
    }
  }

  // persist delete in DB
  try {
    DatabaseManager::getInstance().deleteRoom(deletedRoomId);
  } catch (const std::exception &e) {
    Logger::logError("RoomManager", "Failed to delete room from DB: " + std::string(e.what()));
    return false;
  }

  // move live sessions to Lobby and persist membership for authenticated users
  auto sessions = connections.getSessions();
  DatabaseManager &db = DatabaseManager::getInstance();
  for (SOCKET socket : sockets) {
    auto it = sessions.find(socket);
    if (it == sessions.end() || !it->second) {
      continue;
    }

    ClientSession &session = *it->second;
    session.setRoom(LOBBY);

    if (!session.isAuthenticated()) {
      continue;
    }

    const std::string username = session.getUser().getUsername();
    if (username.empty()) {
      continue;
    }

    try {
      db.clearMembership(username, deletedRoomId);
      db.setMembership(username, LOBBY.getId(), config::NODE_ID);
    } catch (const std::exception &e) {
      Logger::logError("RoomManager",
                       "Failed to persist Lobby move for " + username + ": " + e.what());
    }
  }

  Packet lobbyPush("server", "*", Packet::PacketType::ROOM_LIST, LOBBY.getName(),
                   Room::serializeList(listRooms()), 0);
  for (SOCKET socket : sockets) {
    connections.sendPacket(socket, lobbyPush);
  }

  Logger::logInfo("RoomManager", "Room deleted: " + roomName);
  return true;
}

// broadcast a message to all members of all rooms
bool RoomManager::broadcastAll(const Packet &packet, ConnectionManager &connections,
                               SOCKET skipSocket) {
  std::vector<Room> rooms;

  // get all rooms
  {
    std::lock_guard<std::mutex> lock(mutex);

    // get all rooms
    rooms.reserve(members.size());
    for (const auto &[roomName, _] : members) {
      auto it = knownRooms.find(roomName);
      if (it != knownRooms.end()) {
        rooms.push_back(it->second);
      }
    }
  }

  // broadcast the packet to all sockets in all rooms
  bool ok = true;
  for (const Room &room : rooms) {
    if (!broadcast(room, packet, connections, skipSocket)) {
      ok = false;
    }
  }
  return ok;
}

// list all known rooms
std::vector<Room> RoomManager::listRooms() const {
  std::lock_guard<std::mutex> lock(mutex);

  // get all rooms and return them
  std::vector<Room> rooms;
  rooms.reserve(knownRooms.size());
  for (const auto &[_, room] : knownRooms) {
    rooms.push_back(room);
  }

  return rooms;
}