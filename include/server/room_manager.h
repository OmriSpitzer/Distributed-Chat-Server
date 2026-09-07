/**
 * RoomManager header file class
 *
 * @date 07-09-2026
 */
#pragma once
#include "utils/models/packet.h"
#include "utils/models/room.h"
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

class ClientSession;
class ConnectionManager;

class RoomManager {
public:
  static RoomManager &getInstance() {
    static RoomManager instance;
    return instance;
  }

  RoomManager(const RoomManager &) = delete;
  RoomManager &operator=(const RoomManager &) = delete;

  // look up a known room by name
  std::optional<Room> getRoom(const std::string &name) const;

  // move the session into a known room and record its socket
  bool joinRoom(const std::string &roomName, ClientSession &session);

  // remove the session socket from whatever room it is in (no Lobby insert)
  void leaveAll(ClientSession &session);

  // broadcast a packet to sockets currently in the room
  bool broadcast(const Room &room, const Packet &packet, ConnectionManager &connections,
                 int skipSocket = -1);

  // create a room
  bool createRoom(const Room &room);

  // delete a room
  bool deleteRoom(const Room &room);

  // broadcast a message to all rooms
  bool broadcastAll(const std::string &message);

  // Lobby room
  static const Room LOBBY;

private:
  RoomManager();

  std::unordered_map<std::string, Room> knownRooms;
  std::unordered_map<std::string, std::unordered_set<int>> members;
  mutable std::mutex mutex;

  void removeSocketLocked(int socket, const std::string &roomName);
};