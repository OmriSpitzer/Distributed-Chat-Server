/**
 * RoomManager header file class
 *
 * @date 13-09-2026
 */

#pragma once
#include "utils/models/packet.h"
#include "utils/models/room.h"
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <winsock2.h>
#ifdef ERROR
#undef ERROR
#endif

class ClientSession;
class ConnectionManager;

class RoomManager {
public:
  // singleton instance getter
  static RoomManager &getInstance() {
    static RoomManager instance;
    return instance;
  }

  // delete copy constructor and assignment operator
  RoomManager(const RoomManager &) = delete;
  RoomManager &operator=(const RoomManager &) = delete;

  // room getter
  std::optional<Room> getRoom(const std::string &name) const;

  // join a room
  bool joinRoom(const std::string &roomName, ClientSession &session);

  // leave all rooms
  void leaveAll(ClientSession &session);

  // broadcast a packet to all members of the room
  bool broadcast(const Room &room, const Packet &packet, ConnectionManager &connections,
                 SOCKET skipSocket = INVALID_SOCKET);

  // broadcast a message to all members of all rooms
  bool broadcastAll(const Packet &packet, ConnectionManager &connections,
                    SOCKET skipSocket = INVALID_SOCKET);

  // create a new room
  bool createRoom(const Room &room, std::string_view creatorEmail = "");

  // delete an existing room
  bool deleteRoom(const std::string &roomName, ConnectionManager &connections);

  // default rooms
  static const Room LOBBY;
  static const Room GENERAL;

  // list all known rooms
  std::vector<Room> listRooms() const;

private:
  // constructor
  RoomManager();

  std::unordered_map<std::string, Room> knownRooms; // mapped all known rooms
  std::unordered_map<std::string, std::unordered_set<SOCKET>>
      members;              // mapped all members of all rooms
  mutable std::mutex mutex; // mutex

  // remove a socket from a room
  void removeSocketLocked(SOCKET socket, const std::string &roomName);
};