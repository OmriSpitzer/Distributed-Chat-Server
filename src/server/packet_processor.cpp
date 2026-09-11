/**
 * PacketProcessor class
 *
 * @brief Routes incoming packets to the appropriate manager and builds a response.
 * @date 10-09-2026
 */

#include "server/packet_processor.h"
#include "config/config.h"
#include "server/connection_manager.h"
#include "server/database_manager.h"
#include "server/message_manager.h"
#include "server/room_manager.h"
#include "utils/RESPONSE_CODES.h"
#include "utils/models/user.h"
#include <any>
#include <atomic>
#include <cstdint>
#include <ctime>
#include <exception>
#include <string>
#include <string_view>

namespace {
std::atomic<std::uint64_t> nextPresenceSeq{0};

std::string makeEventId(const std::string &kind, const std::string &username) {
  return config::NODE_ID + "-" + kind + "-" + username + "-" +
         std::to_string(static_cast<long long>(std::time(nullptr))) + "-" +
         std::to_string(++nextPresenceSeq);
}

void rumorEvent(ConnectionManager &connections, const std::string &type,
                const std::string &username, const std::string &content, const std::string &field5,
                const std::string &room = "") {
  const std::string eventId = makeEventId(type, username);
  const std::string payload = type + "|" + eventId + "|" + username + "|" + content + "|" + field5;
  Packet event(config::NODE_ID, "*", Packet::PacketType::GOSSIP_EVENT, room, payload);
  connections.rumor(event);
}

void rumorPresence(ConnectionManager &connections, const char *kind, const std::string &username) {
  const std::string ts = std::to_string(static_cast<long long>(std::time(nullptr)));
  rumorEvent(connections, kind, username, config::NODE_ID, ts);
}

void rumorUserCreated(ConnectionManager &connections, const std::string &username,
                      std::string_view password, std::string_view email) {
  // field5 = email (not a timestamp); do not log this payload
  rumorEvent(connections, "USER_CREATED", username, std::string(password), std::string(email));
}

void rumorRoomJoin(ConnectionManager &connections, const std::string &username,
                   const std::string &newRoom, const std::string &prevRoom) {
  rumorEvent(connections, "ROOM_JOIN", username, config::NODE_ID, prevRoom, newRoom);
}
} // namespace

// process a packet
Packet PacketProcessor::processPacket(const Packet &packet, ClientSession &session,
                                      ConnectionManager &connections) {
  // create a response packet
  Packet response = packet.copy();
  response.sender = "server";
  response.receiver = packet.sender;

  // process the packet
  switch (packet.type) {
    // gossip packets — handled in GossipManager peer loop, not here
  case Packet::PacketType::GOSSIP_HELLO:
  case Packet::PacketType::GOSSIP_EVENT:
  case Packet::PacketType::GOSSIP_DIGEST:
  case Packet::PacketType::GOSSIP_PULL:
    break;

    // login packet
  case Packet::PacketType::LOGIN: {
    try {
      // get the user from the database
      std::any dbResult = DatabaseManager::getInstance().loginUser(packet.sender, packet.message);
      if (const auto *error = std::any_cast<std::string>(&dbResult)) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = *error;
        break;
      }

      User user = std::any_cast<User>(dbResult);

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
      RoomManager::getInstance().joinRoom("Lobby", session);
      rumorPresence(connections, "LOGIN", user.getUsername());
      rumorRoomJoin(connections, user.getUsername(), "Lobby", prevRoom);

      response.responseCode = static_cast<int>(RESPONSE_CODES::SUCCESS);
      response.message = user.serialize();
    } catch (const std::exception &e) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::INTERNAL_SERVER_ERROR);
      response.message = std::string("login failed: ") + e.what();
    }
    break;
  }

    // register packet
  case Packet::PacketType::REGISTER: {
    try {
      if (DatabaseManager::getInstance().userExists(packet.sender)) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "user already exists";
        break;
      }

      // parse the username, password and email
      std::string_view username = packet.sender;
      std::string_view password = packet.message.substr(0, packet.message.rfind('|'));
      std::string_view email = packet.message.substr(packet.message.rfind('|') + 1);

      // create the user
      User user = DatabaseManager::getInstance().createUser(username, password, email);
      rumorUserCreated(connections, user.getUsername(), password, email);

      // set the session
      const std::string prevRoom = session.getRoom().getName();
      session.setUser(user);
      session.setAuthenticated(true);
      RoomManager::getInstance().joinRoom("Lobby", session);
      rumorPresence(connections, "LOGIN", user.getUsername());
      rumorRoomJoin(connections, user.getUsername(), "Lobby", prevRoom);

      response.responseCode = static_cast<int>(RESPONSE_CODES::SUCCESS);
      response.message = user.serialize();
    } catch (const std::exception &e) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::INTERNAL_SERVER_ERROR);
      response.message = std::string("register failed: ") + e.what();
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

    if (wasAuth && !username.empty()) {
      rumorPresence(connections, "LOGOUT", username);
    }

    response.responseCode = static_cast<int>(RESPONSE_CODES::SUCCESS);
    response.message = "logged out";
    break;
  }

  // message packet — MessageManager::send → gossip.rumor (applyChatMessage path)
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
      Room room = session.getRoom();
      const Message stored(session.getUser(), User::anonymousUser(), packet.message);
      if (!MessageManager().send(stored, room, connections, session.getSocket())) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "failed to send message";
        break;
      }

      response.responseCode = static_cast<int>(RESPONSE_CODES::SUCCESS);
      response.message = "ok";
    } catch (const std::exception &e) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::INTERNAL_SERVER_ERROR);
      response.message = std::string("send failed: ") + e.what();
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
      const std::string roomName = packet.room.empty() ? "Lobby" : packet.room;
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
    } catch (const std::exception &e) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::INTERNAL_SERVER_ERROR);
      response.message = std::string("join failed: ") + e.what();
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
      if (!RoomManager::getInstance().joinRoom("Lobby", session)) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "failed to leave room";
        break;
      }

      rumorRoomJoin(connections, session.getUser().getUsername(), "Lobby", prevRoom);

      response.room = "Lobby";
      response.responseCode = static_cast<int>(RESPONSE_CODES::SUCCESS);
      response.message = "left room, back in Lobby";
    } catch (const std::exception &e) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::INTERNAL_SERVER_ERROR);
      response.message = std::string("leave failed: ") + e.what();
    }
    break;
  }

  case Packet::PacketType::HEARTBEAT:
    response.message = "pong";
    break;
  }

  return response;
}

// process a heartbeat packet
Packet PacketProcessor::processHeartbeatPacket(const Packet &packet,
                                               ConnectionManager &connections) {
  auto sessions = connections.getSessions();

  std::string message = "sessions: ";
  for (const auto &entry : sessions) {
    const ClientSession &session = *entry.second;
    message += "[" + session.getUser().getUsername() + "," + session.getRoom().getName() + "] ";
  }
  message += "total: " + std::to_string(sessions.size());

  Packet response = packet.copy();
  response.sender = "server";
  response.receiver = packet.sender;
  response.message = message;
  return response;
}
