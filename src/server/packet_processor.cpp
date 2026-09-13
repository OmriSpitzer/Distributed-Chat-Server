/**
 * PacketProcessor class
 *
 * @brief Routes incoming packets to the appropriate manager and builds a response.
 * @date 13-09-2026
 */

#include "server/packet_processor.h"
#include "config/config.h"
#include "server/connection_manager.h"
#include "server/database_manager.h"
#include "server/room_manager.h"
#include "utils/RESPONSE_CODES.h"
#include "utils/gossip_payload.h"
#include "utils/models/user.h"
#include <atomic>
#include <cstdint>
#include <ctime>
#include <string>
#include <string_view>
#include <variant>

namespace {
std::atomic<std::uint64_t> nextPresenceSeq{0};

// generate a unique event id
std::string makeEventId(const std::string &kind, const std::string &username) {
  return config::NODE_ID + "-" + kind + "-" + username + "-" +
         std::to_string(static_cast<long long>(std::time(nullptr))) + "-" +
         std::to_string(++nextPresenceSeq);
}

// send an event to the network
void rumorEvent(ConnectionManager &connections, const std::string &type,
                const std::string &username, const std::string &content, const std::string &field5,
                const std::string &room = "") {
  const std::string eventId = makeEventId(type, username);
  const std::string payload = gossip_payload::encode(type, eventId, username, content, field5);
  Packet event(config::NODE_ID, "*", Packet::PacketType::GOSSIP_EVENT, room, payload);
  connections.rumor(event);
}

// send a presence event to the network
void rumorPresence(ConnectionManager &connections, const char *kind, const std::string &username) {
  const std::string ts = std::to_string(static_cast<long long>(std::time(nullptr)));
  rumorEvent(connections, kind, username, config::NODE_ID, ts);
}

// send a user created event to the network
void rumorUserCreated(ConnectionManager &connections, const std::string &username,
                      std::string_view password, std::string_view email) {
  rumorEvent(connections, "USER_CREATED", username, std::string(password), std::string(email));
}

// send a room join event to the network
void rumorRoomJoin(ConnectionManager &connections, const std::string &username,
                   const std::string &newRoom, const std::string &prevRoom) {
  rumorEvent(connections, "ROOM_JOIN", username, config::NODE_ID, prevRoom, newRoom);
}

// send a message event to the network
void rumorMessage(ConnectionManager &connections, const std::string &username,
                  const std::string &content, const std::string &room) {
  const std::string ts = std::to_string(static_cast<long long>(std::time(nullptr)));
  rumorEvent(connections, "MESSAGE", username, content, ts, room);
}
} // namespace

// process a packet
Packet PacketProcessor::processPacket(const Packet &packet, ClientSession &session,
                                      ConnectionManager &connections) {
  // create a response packet
  Packet response = Packet(packet);
  response.timestamp = static_cast<uint64_t>(std::time(nullptr));
  response.sender = "server";
  response.receiver = packet.sender;

  // process the packet
  switch (packet.type) {

    // heartbeat packet
  case Packet::PacketType::HEARTBEAT: {
    response.message = "pong";
    response.responseCode = static_cast<int>(RESPONSE_CODES::SUCCESS);
    break;
  }

    // gossip packets
  case Packet::PacketType::GOSSIP_HELLO:
  case Packet::PacketType::GOSSIP_EVENT:
  case Packet::PacketType::GOSSIP_DIGEST:
  case Packet::PacketType::GOSSIP_PULL: {
    response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
    response.message = "unsupported on client port";
    break;
  }

    // login packet
  case Packet::PacketType::LOGIN: {
    try {
      // get the user from the database
      std::variant<User, std::string> dbResult =
          DatabaseManager::getInstance().loginUser(packet.sender, packet.message);
      if (const auto *error = std::get_if<std::string>(&dbResult)) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = *error;
        break;
      }

      User user = std::get<User>(dbResult);

      // local socket or another node's presence row
      if (connections.hasSession(user) ||
          DatabaseManager::getInstance().isUserOnline(user.getUsername())) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "user already logged in";
        break;
      }

      const std::string prevRoom = session.getRoom().getName();
      session.setUser(user);
      session.setAuthenticated(true);
      RoomManager::getInstance().joinRoom(RoomManager::LOBBY.getName(), session);
      rumorPresence(connections, "LOGIN", user.getUsername());
      rumorRoomJoin(connections, user.getUsername(), RoomManager::LOBBY.getName(), prevRoom);

      response.responseCode = static_cast<int>(RESPONSE_CODES::SUCCESS);
      response.message = user.serialize();
    } catch (...) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::INTERNAL_SERVER_ERROR);
      response.message = std::string("login failed");
    }
    break;
  }

    // register packet
  case Packet::PacketType::REGISTER: {
    try {
      // check if the user already exists
      if (DatabaseManager::getInstance().userExists(packet.sender)) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "user already exists";
        break;
      }

      // parse the username, password and email
      std::string_view username = packet.sender;
      std::string_view password = packet.message;
      std::string_view email = packet.room;

      // check if the username, password and email are not empty
      if (username.empty() || password.empty() || email.empty()) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "missing username, password, or email";
        break;
      }

      // create the user
      User user = DatabaseManager::getInstance().createUser(username, password, email);
      rumorUserCreated(connections, user.getUsername(), password, email);

      // set the session
      const std::string prevRoom = session.getRoom().getName();
      session.setUser(user);
      session.setAuthenticated(true);
      RoomManager::getInstance().joinRoom(RoomManager::LOBBY.getName(), session);
      rumorPresence(connections, "LOGIN", user.getUsername());
      rumorRoomJoin(connections, user.getUsername(), RoomManager::LOBBY.getName(), prevRoom);

      response.responseCode = static_cast<int>(RESPONSE_CODES::SUCCESS);
      response.message = user.serialize();
    } catch (...) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::INTERNAL_SERVER_ERROR);
      response.message = std::string("register failed");
    }
    break;
  }

    // logout packet
  case Packet::PacketType::LOGOUT: {
    const bool wasAuth = session.isAuthenticated();
    const std::string username = session.getUser().getUsername();

    RoomManager::getInstance().leaveAll(session);
    session.setAuthenticated(false);
    session.setUser(User::anonymousUser());
    // keep connected anonymous socket in Lobby for live broadcasts
    RoomManager::getInstance().joinRoom(RoomManager::LOBBY.getName(), session);

    if (wasAuth && !username.empty()) {
      rumorPresence(connections, "LOGOUT", username);
    }

    response.responseCode = static_cast<int>(RESPONSE_CODES::SUCCESS);
    response.message = "logged out";
    break;
  }

  // message packet — rumorEvent → GossipManager::applyEvent persist + room broadcast
  case Packet::PacketType::MESSAGE: {
    if (!session.isAuthenticated()) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
      response.message = "not authenticated";
      break;
    }

    if (packet.message.empty()) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
      response.message = "empty message";
      break;
    }

    try {
      rumorMessage(connections, session.getUser().getUsername(), packet.message,
                   session.getRoom().getName());
      response.responseCode = static_cast<int>(RESPONSE_CODES::SUCCESS);
      response.message = "ok";
    } catch (...) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::INTERNAL_SERVER_ERROR);
      response.message = std::string("send failed");
    }
    break;
  }

    // room join packet
  case Packet::PacketType::ROOM_JOIN: {
    if (!session.isAuthenticated()) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
      response.message = "not authenticated";
      break;
    }

    try {
      const std::string roomName = packet.room.empty() ? RoomManager::LOBBY.getName() : packet.room;
      if (!RoomManager::getInstance().getRoom(roomName)) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::NOT_FOUND);
        response.message = "unknown room: " + roomName;
        break;
      }

      const std::string prevRoom = session.getRoom().getName();
      if (!RoomManager::getInstance().joinRoom(roomName, session)) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "failed to join " + roomName;
        break;
      }

      rumorRoomJoin(connections, session.getUser().getUsername(), roomName, prevRoom);

      response.room = roomName;
      response.responseCode = static_cast<int>(RESPONSE_CODES::SUCCESS);
      response.message = "joined " + roomName;
    } catch (...) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::INTERNAL_SERVER_ERROR);
      response.message = std::string("join failed");
    }
    break;
  }

    // room leave packet — returns to Lobby; membership replica via ROOM_JOIN
  case Packet::PacketType::ROOM_LEAVE: {
    if (!session.isAuthenticated()) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
      response.message = "not authenticated";
      break;
    }

    try {
      const std::string prevRoom = session.getRoom().getName();
      if (!RoomManager::getInstance().joinRoom(RoomManager::LOBBY.getName(), session)) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "failed to leave room";
        break;
      }

      rumorRoomJoin(connections, session.getUser().getUsername(), RoomManager::LOBBY.getName(),
                    prevRoom);

      response.room = RoomManager::LOBBY.getName();
      response.responseCode = static_cast<int>(RESPONSE_CODES::SUCCESS);
      response.message = "left room, back in Lobby";
    } catch (...) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::INTERNAL_SERVER_ERROR);
      response.message = std::string("leave failed");
    }
    break;
  }

  default: {
    response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
    response.message = "unknown packet type";
    break;
  }
  }

  return response;
}