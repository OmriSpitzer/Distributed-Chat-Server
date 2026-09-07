/**
 * PacketProcessor class
 *
 * @brief Routes incoming packets to the appropriate manager and builds a response.
 * @date 03-09-2026
 */

#include "server/packet_processor.h"
#include "server/connection_manager.h"
#include "server/database_manager.h"
#include "server/room_manager.h"
#include "utils/RESPONSE_CODES.h"
#include "utils/models/user.h"
#include <any>
#include <exception>
#include <string>

// process a packet
Packet PacketProcessor::processPacket(const Packet &packet, ClientSession &session,
                                      ConnectionManager &connections) {
  // create a response packet
  Packet response = packet.copy();
  response.sender = "server";
  response.receiver = packet.sender;

  // process the packet
  switch (packet.type) {

    // login packet
  case Packet::PacketType::LOGIN: {
    try {
      // get the user from the database
      std::any dbResult = DatabaseManager::getInstance().getUser(packet.sender, packet.message);
      if (const auto *error = std::any_cast<std::string>(&dbResult)) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = *error;
        break;
      }

      User user = std::any_cast<User>(dbResult);

      // check if there is a session of the same user
      if (connections.hasSession(user)) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "user already logged in";
        break;
      }

      session.setUser(user);
      session.setAuthenticated(true);
      RoomManager::getInstance().joinRoom("Lobby", session);

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
      // get the user from the database
      std::any dbResult = DatabaseManager::getInstance().getUser(packet.sender, packet.message);
      if (const auto *user = std::any_cast<User>(&dbResult)) {
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

      // set the session
      session.setUser(user);
      session.setAuthenticated(true);
      RoomManager::getInstance().joinRoom("Lobby", session);

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
    session.setAuthenticated(false);
    session.setUser(User::anonymousUser());
    RoomManager::getInstance().leaveAll(session);

    response.responseCode = static_cast<int>(RESPONSE_CODES::SUCCESS);
    response.message = "logged out";
    break;
  }

  // message packet
  case Packet::PacketType::MESSAGE: {
    // check if the session is authenticated
    if (!session.isAuthenticated()) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
      response.message = "not authenticated";
      break;
    }

    // check if the message is not empty
    if (packet.message.empty()) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
      response.message = "empty message";
      break;
    }

    // get the room
    const Room &room = session.getRoom();

    Message stored(session.getUser(), User::anonymousUser(), packet.message);

    // TODO: save the message to the database

    // create a push packet
    Packet push = packet.copy();
    push.sender = session.getUser().getUsername();
    push.receiver = "";
    push.room = room.getName();
    push.message = packet.message;
    push.type = Packet::PacketType::MESSAGE;
    push.responseCode = 0;

    // broadcast the message to the room
    RoomManager::getInstance().broadcast(room, push, connections, session.getSocket());

    // ack only
    response.responseCode = static_cast<int>(RESPONSE_CODES::SUCCESS);
    response.message = "ok";
    break;
  }

    // room join packet
  case Packet::PacketType::ROOM_JOIN: {
    if (!session.isAuthenticated()) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
      response.message = "not authenticated";
      break;
    }

    const std::string &roomName = packet.room.empty() ? "Lobby" : packet.room;
    if (!RoomManager::getInstance().getRoom(roomName)) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::NOT_FOUND);
      response.message = "unknown room: " + roomName;
      break;
    }

    if (!RoomManager::getInstance().joinRoom(roomName, session)) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
      response.message = "failed to join " + roomName;
      break;
    }

    response.room = roomName;
    response.responseCode = static_cast<int>(RESPONSE_CODES::SUCCESS);
    response.message = "joined " + roomName;
    break;
  }

    // room leave packet
  case Packet::PacketType::ROOM_LEAVE: {
    if (!session.isAuthenticated()) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
      response.message = "not authenticated";
      break;
    }

    if (!RoomManager::getInstance().joinRoom("Lobby", session)) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
      response.message = "failed to leave room";
      break;
    }

    response.room = "Lobby";
    response.responseCode = static_cast<int>(RESPONSE_CODES::SUCCESS);
    response.message = "left room, back in Lobby";
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
